# Build help (beginner-friendly)

This branch makes the project easier to build on a typical Android Studio install.

## What we changed
1. **Gradle / AGP** — moved from bleeding-edge AGP 9 to **AGP 8.7.3 + Gradle 8.11.1 + Kotlin 2.0**, which works with more Android Studio versions.
2. **Internet permission** — added so Firebase multiplayer can talk to the network once configured.
3. **Theme** — switched to `NoActionBar` to avoid window conflicts with `GameActivity`.
4. **Safer native load** — if the C++ library fails to load, you get a Toast instead of a silent crash.
5. **Google Services plugin enabled** — `alias(libs.plugins.google.services)` is on in `app/build.gradle.kts`. You still need a local `app/google-services.json` from your Firebase project for Host / Join to work.

## What you should do in Android Studio
1. **Pull / sync this branch** (or merge the PR into `master`).
2. After merge, sync your local tree to remote master:
   ```bash
   git fetch && git reset --hard origin/master
   ```
   Then **Build → Clean Project** and **Rebuild** (especially important when native C++ changed).
3. **File → Sync Project with Gradle Files**.
4. Install tools if prompted:
   - **JDK 17** (Android Studio usually bundles one)
   - **SDK Platform 35**
   - **NDK** and **CMake** (Tools → SDK Manager → SDK Tools)
5. Place `app/google-services.json` if you want online Host / Join (see `FIREBASE_SETUP.md`).
6. **Build → Rebuild Project**.
7. Run on an emulator or phone with **API 30+**.

## If it still fails
Copy the **Build** output (the red error text) and share it. The most useful lines are the first `error:` / `FAILED` block.

## Multiplayer
1. Create a Firebase project and download `google-services.json`.
2. Put it in the `app/` folder (**required locally** — the Google Services plugin is already enabled).
3. Enable Realtime Database (see `FIREBASE_SETUP.md`).
4. Sync and rebuild.
5. **Host**: enter your **DM name only** (no class pick) → share the session ID. Friends **Join**, pick a class, and wait in the lobby.

## 16 KB page size (Android Studio warning)
This project pins **NDK 28.2.13676358**, enables flexible page sizes in CMake, and packages JNI libs without legacy compression so 64-bit `.so` files are 16 KB-aligned.

In Android Studio: **Tools → SDK Manager → SDK Tools →** install/update **NDK (Side by side)** so version `28.2.13676358` is present, then **Build → Clean Project** and **Rebuild**.

### If APK Analyzer still says 4 KB
The native build cache is sticky. Do a hard clean:

1. Confirm NDK **28.2.13676358** is installed (SDK Manager → SDK Tools → Show Package Details).
2. Close the app if running.
3. In a terminal in the project folder, run:
   ```bash
   rm -rf app/.cxx app/build build
   ```
4. In Android Studio: **File → Sync Project with Gradle Files**
5. **Build → Rebuild Project**
6. Analyze the new `app-debug.apk` again — `libdndbeginnerremote.so` should no longer say 4 KB.
