# Contributing

Thanks for helping! Bug reports, fixes and features are all welcome.

## Sending changes

- A pull request straight away is fine. For a new feature, opening an issue first is appreciated, so we can
  agree on it before you put in the work.
- Build and test: see [Building](README.md#building) and [Tests](README.md#tests) in the README. Run
  `ctest --preset conan-debug` before sending.
- Keep a pull request to one topic.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/): `type(scope): what it does`

```
feat(snapshots): export a world's snapshots to a bundle
fix(game): don't hang when choosing a userdata folder
```

Scopes are the tab or core area you touched: `game`, `mods`, `soundpacks`, `fonts`, `backups`, `snapshots`,
`trash`, `settings`, `releases`, `launcher`, `ui`.

## Code rules

- `src/core/` never includes Qt, so it can be tested without a window.
- Slow work (downloads, installs, backups) runs on the worker thread through `TaskRunner`
  (`src/ui/task_runner.h`), never on the Qt main (GUI) thread.
- Every delete or replace asks the user to confirm first.
- Files that belong to the game (saves, configs, mod folders) are read and written in the game's own format.
  Don't invent launcher-only fields or layouts for them.

## Bugs

Please include:

- the game and fork (DDA, BN, ...) and the build you had installed
- the launcher commit (`git rev-parse --short HEAD`)
- the steps to reproduce it
- the terminal output from the launcher

## License

By contributing you agree that your contributions are released under the [MIT license](LICENSE).
