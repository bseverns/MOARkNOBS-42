#!/usr/bin/env node
/*
 * Persistence abuse evidence wrapper for the current MN42 hardware-test stack.
 *
 * Safe-by-default behavior:
 *  - runs the non-destructive boot/apply/readback proof when a serial port is
 *    provided
 *  - only runs destructive profile/macro/scene storage checks when
 *    --exercise-storage is explicitly passed
 *
 * Corruption and interrupted-write scenarios that require fault injection are
 * reported as Unity-covered or manual-only until the repo has explicit safe
 * hooks for them.
 */

'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
const repoRoot = path.resolve(__dirname, '../..');
const logsDir = path.resolve(repoRoot, 'logs');

function argValue(flag, fallback) {
  const index = args.indexOf(flag);
  if (index >= 0 && index + 1 < args.length) {
    return args[index + 1];
  }
  return fallback;
}

function hasFlag(flag) {
  return args.includes(flag);
}

function now() {
  return new Date().toISOString();
}

function stamp() {
  return now().replace(/[:.]/g, '-');
}

function detectPort() {
  const envPort = process.env.MN42_SERIAL || process.env.TEST_PORT;
  if (envPort) return envPort;
  const preferredPrefixes = [
    '/dev/cu.usbmodem',
    '/dev/cu.usbserial',
    '/dev/ttyACM',
    '/dev/ttyUSB',
    '/dev/tty.usbmodem',
    '/dev/tty.usbserial'
  ];
  try {
    const devEntries = fs.readdirSync('/dev').map((entry) => `/dev/${entry}`);
    for (const prefix of preferredPrefixes) {
      const match = devEntries.find((entry) => entry.startsWith(prefix));
      if (match) return match;
    }
  } catch (_) {
    return null;
  }
  return null;
}

function readJsonIfPresent(filePath) {
  if (!filePath || !fs.existsSync(filePath)) return null;
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function scenario(id, title, fields = {}) {
  return {
    id,
    title,
    at: now(),
    ...fields
  };
}

function summarizeError(error) {
  if (!error) return 'unknown error';
  return error.message || String(error);
}

function spawnLogged(command, commandArgs, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, commandArgs, {
      cwd: options.cwd || repoRoot,
      env: { ...process.env, ...(options.env || {}) },
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => {
      const text = chunk.toString();
      stdout += text;
      process.stdout.write(text);
    });
    child.stderr.on('data', (chunk) => {
      const text = chunk.toString();
      stderr += text;
      process.stderr.write(text);
    });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve({ stdout, stderr, code, signal });
        return;
      }
      reject(
        new Error(
          `${command} ${commandArgs.join(' ')} failed with ${
            signal ? `signal ${signal}` : `exit ${code}`
          }`
        )
      );
    });
  });
}

async function main() {
  const serialPath = argValue('--serial', detectPort());
  const reportPath = argValue(
    '--report',
    path.resolve(logsDir, `persistence-abuse-${stamp()}.json`)
  );
  const profileSlot = parseInt(argValue('--profile-slot', process.env.MN42_PROFILE_SLOT || '3'), 10);
  const sceneSlot = parseInt(argValue('--scene-slot', process.env.MN42_SCENE_SLOT || '5'), 10);
  const potIndex = parseInt(argValue('--pot-index', process.env.MN42_POT_INDEX || '0'), 10);
  const exerciseStorage =
    hasFlag('--exercise-storage') || process.env.MN42_EXERCISE_STORAGE === '1';
  const shouldFlashBoot =
    hasFlag('--flash') || process.env.MN42_FLASH_BEFORE_BOOT_TEST === '1';
  const skipBoot = hasFlag('--skip-boot-proof');

  const bootReportPath = argValue(
    '--boot-report',
    path.resolve(logsDir, `persistence-boot-proof-${stamp()}.json`)
  );
  const storageReportPath = argValue(
    '--storage-report',
    path.resolve(logsDir, `persistence-storage-proof-${stamp()}.json`)
  );

  const bootRunnerPath = path.resolve(__dirname, 'mn42_boot_contract_runner.js');
  const fullstackRunnerPath = path.resolve(__dirname, 'mn42_fullstack_runner.js');

  const report = {
    started_at: now(),
    serial_port: serialPath || null,
    destructive_storage_armed: exerciseStorage,
    commands: [],
    artifacts: {
      report: reportPath,
      boot_report: skipBoot ? null : bootReportPath,
      storage_report: exerciseStorage ? storageReportPath : null
    },
    scenarios: [
      scenario('primary-good-backup-corrupt', 'Primary copy survives backup corruption', {
        status: 'covered_by_unity',
        automation: 'unity',
        command: 'pio test -d firmware -e teensy40_unity -vvv',
        evidence:
          'firmware/test/test_config_persistence.cpp:test_config_load_prefers_primary_when_backup_copy_is_invalid',
        notes: 'Host runner does not scribble corrupt backup bytes onto production storage.'
      }),
      scenario('primary-corrupt-backup-good', 'Backup copy restores and repairs primary', {
        status: 'covered_by_unity',
        automation: 'unity',
        command: 'pio test -d firmware -e teensy40_unity -vvv',
        evidence:
          'firmware/test/test_config_persistence.cpp:test_config_load_restores_from_backup_and_repairs_primary',
        notes:
          'Recovery is automated in the in-memory storage backend, not by corrupting attached hardware from the host.'
      }),
      scenario('both-corrupt-defaults-warning', 'Both corrupt copies fall back to defaults with warning', {
        status: 'covered_by_unity',
        automation: 'unity',
        command: 'pio test -d firmware -e teensy40_unity -vvv',
        evidence:
          'firmware/test/test_config_persistence.cpp:test_config_load_resets_to_defaults_when_primary_and_backup_are_both_corrupt',
        notes: 'Production-board corruption injection remains intentionally manual until safe fault hooks exist.'
      }),
      scenario('profile-save-interrupted', 'Interrupted profile save leaves latest copy recoverable', {
        status: 'covered_by_unity',
        automation: 'unity',
        command: 'pio test -d firmware -e teensy40_unity -vvv',
        evidence:
          'firmware/test/test_config_persistence.cpp:test_profile_save_interruption_leaves_latest_copy_in_backup',
        notes: 'Simulates primary write loss while preserving the backup-first save ordering.'
      }),
      scenario('reset-before-ack', 'Reset or disconnect before ACK', {
        status: 'manual_only',
        automation: 'manual',
        command: 'Use docs/bench/firmware/PersistenceAbuse.md operator steps or a future fault-injection hook.',
        notes: 'No safe production-host hook exists to cut power or force disconnect mid-SET_ALL.'
      }),
      scenario('reset-after-ack', 'Reset or disconnect after ACK', {
        status: 'manual_only',
        automation: 'manual',
        command: 'Use docs/bench/firmware/PersistenceAbuse.md operator steps or a future scripted power-cycle rig.',
        notes: 'Attach-live boot proof covers readback after apply, but not an injected reset immediately after ACK.'
      }),
      scenario('legacy-config-migration', 'Legacy persisted config migration', {
        status: 'deferred',
        automation: 'deferred',
        command: null,
        notes: 'No dedicated persisted legacy-config fixture is currently scripted in this repo.'
      })
    ]
  };

  try {
    if (!skipBoot) {
      if (!serialPath) {
        report.scenarios.push(
          scenario('normal-apply-reboot-readback', 'Normal config apply + reboot + readback', {
            status: 'skipped',
            automation: 'hil',
            command: null,
            notes: 'No serial port detected. Pass --serial to run the non-destructive boot/apply proof.'
          })
        );
      } else {
        const commandArgs = [
          bootRunnerPath,
          '--serial',
          serialPath,
          '--attach-live',
          '--report',
          bootReportPath
        ];
        if (shouldFlashBoot) commandArgs.push('--flash');
        report.commands.push([process.execPath, ...commandArgs].join(' '));
        await spawnLogged(process.execPath, commandArgs, { cwd: repoRoot });
        const bootReport = readJsonIfPresent(bootReportPath);
        report.scenarios.push(
          scenario('normal-apply-reboot-readback', 'Normal config apply + reboot + readback', {
            status: bootReport?.result === 'passed' ? 'passed' : 'unknown',
            automation: 'hil',
            command: [process.execPath, ...commandArgs].join(' '),
            artifact: bootReportPath,
            evidence: {
              firmware_env: bootReport?.firmware_env ?? 'unknown/not captured',
              manifest_git_sha: bootReport?.manifest?.git_sha ?? 'unknown/not captured',
              checksum: bootReport?.apply?.checksum ?? 'unknown/not captured'
            },
            notes:
              'This is the safe default persistence receipt: staged SET_ALL apply, ACK, readback, and cleanup on the attached board.'
          })
        );
      }
    }

    if (exerciseStorage) {
      if (!serialPath) {
        throw new Error('Cannot run destructive storage checks without --serial or TEST_PORT.');
      }
      const commandArgs = [
        fullstackRunnerPath,
        '--serial',
        serialPath,
        '--exercise-storage',
        '--profile-slot',
        String(profileSlot),
        '--scene-slot',
        String(sceneSlot),
        '--pot-index',
        String(potIndex),
        '--report',
        storageReportPath
      ];
      report.commands.push([process.execPath, ...commandArgs].join(' '));
      await spawnLogged(process.execPath, commandArgs, { cwd: repoRoot });
      const storageReport = readJsonIfPresent(storageReportPath);
      const scenarios = Array.isArray(storageReport?.scenarios) ? storageReport.scenarios : [];
      const storageScenario = scenarios.find((entry) => entry.id === 'storage-smoke');
      report.scenarios.push(
        scenario('profile-save-load-reset', 'Profile save/load/reset on real hardware', {
          status: storageScenario?.ok === true ? 'passed' : 'unknown',
          automation: 'hil_destructive',
          command: [process.execPath, ...commandArgs].join(' '),
          artifact: storageReportPath,
          notes:
            'This lane is intentionally destructive. It overwrites the selected profile slot, macro snapshot, and selected scene slot.'
        })
      );
    } else {
      report.scenarios.push(
        scenario('profile-save-load-reset', 'Profile save/load/reset on real hardware', {
          status: 'skipped',
          automation: 'hil_destructive',
          command: null,
          notes:
            'Re-run with --exercise-storage --profile-slot <sacrificial> --scene-slot <sacrificial> to prove destructive storage flows.'
        })
      );
    }

    report.result = report.scenarios.some((entry) => entry.status === 'failed') ? 'failed' : 'completed';
  } catch (error) {
    report.result = 'failed';
    report.error = summarizeError(error);
    report.scenarios.push(
      scenario('runner-failure', 'Persistence abuse runner execution', {
        status: 'failed',
        automation: 'host',
        notes: summarizeError(error)
      })
    );
  } finally {
    report.completed_at = now();
    fs.mkdirSync(path.dirname(reportPath), { recursive: true });
    fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
    console.log(`[persistence-abuse] wrote report to ${reportPath}`);
    if (report.result === 'failed') {
      process.exitCode = 1;
    }
  }
}

main();
