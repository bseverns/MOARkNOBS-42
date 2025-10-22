# Pre-commit wrangling in the bunker

Running `pre-commit` locally keeps our formatting and linting sharp, but the build
containers in CI and these remote sandboxes live behind a proxy that blocks the
PyPI packages the hooks depend on. That means a plain `pip install pre-commit`
can explode with `Tunnel connection failed: 403 Forbidden` errors, leaving you
without the actual executable.

## Quickstart outside the firewall

If you're hacking on this project outside the hostile proxy:

1. Install the tool once:
   ```bash
   pip install pre-commit
   ```
2. Prime the hooks so the first real run is fast:
   ```bash
   pre-commit install
   pre-commit run --all-files
   ```
3. Kick it every time you touch the tree:
   ```bash
   pre-commit run
   ```

## Surviving inside the proxy

When you get the 403 smackdown, you still need to know whether the hooks would
have liked your patch. Options:

- Run the hook binaries manually if they already exist in `.cache/pre-commit`.
  Clang-format, ESLint, and Prettier can all be called directly on the files you
  touched.
- If the tools are missing, sync the repo on a machine with full internet
  access, run the hooks there, and commit the resulting formatting fixes.
- Worst case, document the failure in your testing notes so reviewers know the
  environment blocked the run.

Stay loud, stay linted. `pre-commit` keeps us honest even when the network cops
are trying to harsh the vibe.
