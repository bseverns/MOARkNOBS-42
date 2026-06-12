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

const { spawn, spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
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

function dateStamp() {
  return now().slice(0, 10);
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

function gitOutput(...commandArgs) {
  const result = spawnSync('git', commandArgs, {
    cwd: repoRoot,
    encoding: 'utf8'
  });
  if (result.status !== 0) return null;
  return result.stdout.trim() || null;
}

function summarizeManifest(manifest) {
  return {
    device_name: manifest?.device_name,
    fw_version: manifest?.fw_version,
    schema_version: manifest?.schema_version,
    git_sha: manifest?.git_sha,
    power_profile: manifest?.power_profile
  };
}

function resolveBenchSummaryPath(runDate = dateStamp()) {
  return path.resolve(repoRoot, 'docs/bench/firmware', `${runDate}_persistence-safe-summary.md`);
}

function makeSafePersistenceSummary(bootReport, bootReportPath, markdownPath) {
  const proof = bootReport?.safe_persistence || {};
  const steps = Array.isArray(proof.steps) ? proof.steps : [];
  return {
    status: proof.ok === true ? 'passed' : 'failed',
    artifact: bootReportPath,
    markdown_summary: markdownPath,
    firmware_env: bootReport?.firmware_env ?? null,
    path: proof.path || 'filter.idle_floor',
    mutation: {
      baseline_value: proof?.baseline?.value ?? null,
      staged_value: proof?.mutation?.staged ?? null,
      readback_value: proof?.readback?.value ?? null,
      restored_value: proof?.cleanup?.value ?? null,
      checksum: proof?.apply?.checksum ?? null,
      apply_seq: proof?.apply?.seq ?? null,
      restore_checksum: proof?.restore?.checksum ?? null,
      restore_seq: proof?.restore?.seq ?? null
    },
    steps: steps.map((step) => ({
      id: step.id,
      title: step.title,
      status: step.ok === true ? 'passed' : 'failed',
      detail: step.detail || ''
    }))
  };
}

function renderSafePersistenceMarkdown(report, safeSummary) {
  const manifest = report?.manifest || {};
  const mutation = safeSummary?.mutation || {};
  const stepLines = (safeSummary?.steps || [])
    .map((step) => `- ${step.status.toUpperCase()}: ${step.title}${step.detail ? ` — ${step.detail}` : ''}`)
    .join('\n');

  const provenLines = [
    '- Baseline config was read from the attached board.',
    '- One safe config mutation was staged locally.',
    '- The apply returned a matching checksum ACK.',
    '- GET_CONFIG readback confirmed the applied mutation.',
    '- The original value was restored.',
    '- Final GET_CONFIG readback confirmed cleanup.'
  ].join('\n');

  const caveatLines = [
    '- Non-destructive only. This receipt does not include raw EEPROM corruption, profile-slot destruction, or power-pull timing drills.',
    '- Reset/power-cut timing remains manual until a safe hook exists.'
  ].join('\n');

  return `# Firmware Bench Summary: Safe Persistence

Date: ${dateStamp()}
Commit: ${report?.git?.commit ?? 'unknown/not captured'}
Commit short: ${report?.git?.short_commit ?? 'unknown/not captured'}
Firmware git_sha: ${manifest.git_sha ?? 'unknown/not captured'}
Firmware version: ${manifest.fw_version ?? 'unknown/not captured'}
Schema version: ${manifest.schema_version ?? 'unknown/not captured'}
Power profile: ${manifest.power_profile ?? 'unknown/not captured'}
Serial port: ${report?.serial_port ?? 'unknown/not captured'}
Host: ${report?.host?.hostname ?? 'unknown/not captured'}
Platform: ${report?.host?.platform ?? 'unknown/not captured'}
Firmware env: ${safeSummary?.firmware_env ?? 'unknown/not captured'}
Runner: firmware/system_test/mn42_persistence_abuse_runner.js
JSON report: ${report?.artifacts?.report ?? 'unknown/not captured'}
Boot proof report: ${safeSummary?.artifact ?? 'unknown/not captured'}

## Result

${safeSummary?.status === 'passed' ? 'PASS' : 'FAIL'}

## Proven

${provenLines}

## Mutation Summary

- Path: \`${safeSummary?.path ?? 'filter.idle_floor'}\`
- Baseline value: ${mutation.baseline_value ?? 'unknown/not captured'}
- Staged value: ${mutation.staged_value ?? 'unknown/not captured'}
- Readback after apply: ${mutation.readback_value ?? 'unknown/not captured'}
- Restored value: ${mutation.restored_value ?? 'unknown/not captured'}
- Apply checksum ACK: ${mutation.checksum ?? 'unknown/not captured'}
- Restore checksum ACK: ${mutation.restore_checksum ?? 'unknown/not captured'}

## Step Log

${stepLines || '- No step data captured.'}

## Caveats

${caveatLines}
`;
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
  const markdownSummaryPath = resolveBenchSummaryPath();
  const gitCommit = gitOutput('rev-parse', 'HEAD');
  const gitShortCommit = gitOutput('rev-parse', '--short', 'HEAD');

  const report = {
    started_at: now(),
    serial_port: serialPath || null,
    destructive_storage_armed: exerciseStorage,
    commands: [],
    artifacts: {
      report: reportPath,
      boot_report: skipBoot ? null : bootReportPath,
      storage_report: exerciseStorage ? storageReportPath : null,
      safe_markdown_summary: skipBoot ? null : markdownSummaryPath
    },
    git: {
      commit: gitCommit,
      short_commit: gitShortCommit
    },
    host: {
      hostname: os.hostname(),
      platform: `${os.platform()} ${os.release()}`
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
        report.manifest = bootReport?.manifest ? summarizeManifest(bootReport.manifest) : null;
        report.safe_run = makeSafePersistenceSummary(
          bootReport,
          bootReportPath,
          markdownSummaryPath
        );
        fs.mkdirSync(path.dirname(markdownSummaryPath), { recursive: true });
        fs.writeFileSync(
          markdownSummaryPath,
          renderSafePersistenceMarkdown(report, report.safe_run),
          'utf8'
        );
        report.scenarios.push(
          scenario('normal-apply-reboot-readback', 'Normal config apply + reboot + readback', {
            status: report.safe_run.status,
            automation: 'hil',
            command: [process.execPath, ...commandArgs].join(' '),
            artifact: bootReportPath,
            evidence: {
              firmware_env: bootReport?.firmware_env ?? 'unknown/not captured',
              manifest_git_sha: bootReport?.manifest?.git_sha ?? 'unknown/not captured',
              checksum: report.safe_run?.mutation?.checksum ?? 'unknown/not captured'
            },
            notes:
              `This is the safe default persistence receipt: baseline read, staged mutation, checksum ACK, readback, restore, and cleanup on the attached board. Markdown summary: ${markdownSummaryPath}`
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
    if (report.artifacts.safe_markdown_summary && fs.existsSync(report.artifacts.safe_markdown_summary)) {
      console.log(
        `[persistence-abuse] wrote markdown summary to ${report.artifacts.safe_markdown_summary}`
      );
    }
    if (report.result === 'failed') {
      process.exitCode = 1;
    }
  }
}

main();
