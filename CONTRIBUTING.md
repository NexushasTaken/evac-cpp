# Contributing

Thanks for helping! Bug reports, fixes and features are all welcome.

## Sending changes

- A pull request straight away is fine. For a new feature, opening an issue first is appreciated, so we can
  agree on it before you put in the work.
- Build and test: see [Building](README.md#building) and [Tests](README.md#tests) in the README. Run
  `ctest --preset conan-debug` before sending.
- Format your changes with [`.clang-format`](.clang-format) before committing: stage them, then run
  `git clang-format`. It formats only the lines you changed, then `git add` the result. Most editors
  (clangd, VS Code, Qt Creator) pick up `.clang-format` on their own.
- Some commits only reformat or rename code. They are listed in
  [`.git-blame-ignore-revs`](.git-blame-ignore-revs). To make `git blame` skip them, run once in your clone:
  `git config blame.ignoreRevsFile .git-blame-ignore-revs`. GitHub's blame view skips them already.
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
- Class and struct data members are named `m_name` (`m_status`, `m_ctx`).
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
