## Deploying the Web App to GitHub Pages

This app is pure static HTML/JS/CSS, so publishing is a matter of copying `App/` into the branch GitHub Pages is configured to serve (for this repo it is `gh-pages` / the `/App` folder depending on your Pages settings).

1. Ensure dependencies are installed so the dev server works locally (required for sanity checks):
   ```bash
   npm --prefix App install
   npm --prefix App test
   ```

2. Choose a Pages branch (`gh-pages` is recommended so `main` stays clean):  
   ```bash
   git worktree add /tmp/gh-pages gh-pages
   rm -rf /tmp/gh-pages/*
   cp -R App/* /tmp/gh-pages/
   ```

3. Commit and push the fresh build:
   ```bash
   cd /tmp/gh-pages
   git add -A
   git commit -m "Publish App UI"
   git push origin gh-pages --force
   ```

4. Verify the GitHub Pages site (`https://bseverns.github.io/MN42/`) loads the new App. If your repo uses a different Pages target (e.g., `/docs`), copy the contents into that directory instead and push to `main`.

5. Optional: automate this by scripting the copy/push step in your release workflow.
