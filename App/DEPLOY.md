## Deploying the Web App

`App/` in this repository is the only source of truth. Do not manually mirror its files into another checkout or Pages branch.

### Validate and build

Use the canonical App validation command under Node 24:

```bash
npm --prefix App run ci
```

That command runs interaction and viewport screenshots, contract synchronization, simulator protocol checks, the deployment build, and the deployment smoke check.

The deployment artifact is generated at `dist/app/`. It contains:

- a root `index.html`;
- a commit-addressed `releases/<source-sha>/` directory containing the static App;
- `deploy-manifest.json` with App version, source identity, clean/dirty state, publishability, and release path.

The root page uses `<base>` to load every relative module, schema, and preset from the commit-addressed directory. This prevents a newly published HTML page from mixing with stale CSS or JavaScript from an older deployment.

CI can override the recorded identity with `APP_SOURCE_SHA`; otherwise the build uses the current Git commit. A local build with App changes is labeled `<sha>-dirty` and marked `publishable: false` so it cannot masquerade as the clean commit:

```bash
APP_SOURCE_SHA="$(git rev-parse HEAD)" npm --prefix App run test:deploy
```

### Publish

Configure one Pages workflow or hosting job to upload `dist/app/` exactly as produced. The publishing job must not copy directly from `App/` or maintain a second hand-edited mirror.

Before publishing, record:

- successful `npm --prefix App run ci` output;
- the `source_sha` from `dist/app/deploy-manifest.json`;
- the matching firmware SHA and hardware App receipt when the release claims physical-device validation.

The publishing job must reject an artifact unless `deploy-manifest.json` contains `"publishable": true`.

After publication, verify the deployed source identity in the connection-details disclosure and confirm:

- the root page loads under the repository subpath;
- module, schema, and preset requests return successfully;
- the simulator connects;
- Stage, Configure, and Lab render without console errors.

Do not publish the Pages mirror until those checks pass and the deployment SHA is recorded.
