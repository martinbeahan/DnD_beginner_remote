# Build help (beginner-friendly)

This branch makes the project easier to build on a typical Android Studio install.

## What we changed
1. **Gradle / AGP** — moved from bleeding-edge AGP 9 to **AGP 8.7.3 + Gradle 8.11.1 + Kotlin 2.0**, which works with more Android Studio versions.
2. **Internet permission** — added so Firebase multiplayer can talk to the network once configured.
3. **Theme** — switched to `NoActionBar` to avoid window conflicts with `GameActivity`.
4. **Safer native load** — if the C++ library fails to load, you get a Toast instead of a silent crash.
5. **Firebase stays optional** — the Google Services plugin stays off until you add `google-services.json`.

## What you should do in Android Studio
1. **Pull / sync this branch** (or merge the PR into `master`).
2. **File → Sync Project with Gradle Files**.
3. Install tools if prompted:
   - **JDK 17** (Android Studio usually bundles one)
   - **SDK Platform 35**
   - **NDK** and **CMake** (Tools → SDK Manager → SDK Tools)
4. **Build → Rebuild Project**.
5. Run on an emulator or phone with **API 30+**.

## If it still fails
Copy the **Build** output (the red error text) and share it. The most useful lines are the first `error:` / `FAILED` block.

## Multiplayer later
1. Create a Firebase project and download `google-services.json`.
2. Put it in the `app/` folder.
3. Uncomment `alias(libs.plugins.google.services)` in `app/build.gradle.kts`.
4. Sync and rebuild.

## 16 KB page size (Android Studio warning)
This project pins **NDK 28.2.13676358**, enables flexible page sizes in CMake, and packages JNI libs without legacy compression so 64-bit `.so` files are 16 KB-aligned.

In Android Studio: **Tools → SDK Manager → SDK Tools →** install/update **NDK (Side by side)** so version `28.2.13676358` is present, then **Build → Clean Project** and **Rebuild**.

