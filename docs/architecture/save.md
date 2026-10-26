# Save

`rtype::save::Save` lives in `libs/save` and has no dependency on the ECS or raylib. It keeps values from one run of the client to the next, each under a key: the settings for now, more later. The `Engine` owns one, reached with `engine.getSave()`.

The values live in memory. `readFile()` replaces them with the content of the file given to the constructor or to `setPath()`; a missing file leaves no value, as on a first run. `writeFile()` writes them, creating the directories of the file. It writes to `<file>.tmp` first and then renames it, so a crash while writing keeps the previous save.

```cpp
rtype::save::Save &save = engine.getSave();
save.setPath("save.txt");
save.readFile();
save.setInt("audio.volume", 80);
const int volume = save.getInt("audio.volume").value_or(100);
save.writeFile();
```

Values are set and read as `String`, `Int`, `Float` or `Bool`. They are stored as text, so a getter returns `std::nullopt` when the key is missing or when its value is not of the requested type: `value_or()` gives the default. `hasKey()` and `removeKey()` complete the API.

The file is text, one `key=value` line per value, sorted by key. A backslash, `=` and line breaks in a key or a value are escaped with a backslash. Lines without `=` are ignored, and Windows line ends are accepted.

Name keys by domain with dots (`audio.volume`, `video.fullscreen`), so that the file stays readable.
