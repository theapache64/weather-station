# weather-station agent rules

## Changelog (required)

Every user-visible change to `docs/` (dashboard UI, chart, dialogs, copy, features) **must** get a changelog entry in the **same change**.

### Where

- File: `docs/changelog.js`
- Newest entries go **first** in the `CHANGELOG` array

### Entry shape

```js
{
  id: "YYYY-MM-DD-short-slug",  // unique, stable — never reuse or rename after ship
  date: "YYYY-MM-DD",           // day the change ships
  title: "Short human title",
  items: [
    "One plain-language bullet per user-facing change",
  ],
}
```

### Rules

1. **Date** is the calendar day of the change (`YYYY-MM-DD`).
2. **`id`** must be unique. Prefer `date + short-slug` (e.g. `2026-08-07-history-range`).
3. **Items** are for visitors — **super concise**, a little funny, plain English. No jargon, no essays. One short line per bullet.
4. **Titles** same vibe: short and dry-humor friendly.
5. Group related work from the same day into one entry when it ships together; split when the user should see them as separate updates.
6. Do **not** remove or rewrite old entries' `id` values — localStorage keys off the latest seen `id`.
7. Firmware-only / non-dashboard changes do not need a changelog entry unless they change what the site shows or means.

### What's New dialog

- Site reads `CHANGELOG` and shows unseen entries (ids after the visitor's last-seen id in localStorage).
- Dismissing the dialog stores the newest entry's `id` under `changelog-seen-id`.
- Force-show real entries (no dummy): open with `?whatsnew=1` (does not write localStorage on dismiss).
