# Localization

`rtype::localization::Localization` lives in `libs/localization` and has no dependency on the ECS or renderer. The client loads UTF-8 catalogs from `apps/client/assets/locales/` at startup. Each line has the form `key=value`; empty lines and lines beginning with `#` are ignored. Duplicate keys and malformed lines fail loading with the file and line number.

The client starts in English. `setLanguage("fr")` switches to French immediately; an unknown language leaves the selection unchanged. `text(key)` searches the selected catalog, then the default English catalog, then returns the key itself. The Settings menu offers a language button that switches between English and French.

Menu buttons and headers that stay open during a language change use `BoundText`, so the ECS refreshes their visible text on the next update. Menus opened later request their text from the current catalog when they build their scene. Add new UI text by adding the same key to both catalog files and requesting it through `Localization::text`.
