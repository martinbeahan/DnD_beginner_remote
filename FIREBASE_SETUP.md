# Firebase setup (online multiplayer)

Online Host / Join needs **Firebase Realtime Database**. The Google Services plugin is **already enabled** in `app/build.gradle.kts`. You still need a local `app/google-services.json` or Host / Join will report Firebase isn’t configured.

## 1. Create a Firebase project
1. Open [Firebase Console](https://console.firebase.google.com/)
2. Add a project (any name)
3. Add an **Android** app with package name: `com.fintrack.dndbeginnerremote`
4. Download `google-services.json` and place it at:
   `app/google-services.json`  
   (**Required locally** for online play.)

## 2. Enable Realtime Database
1. Build → Realtime Database → Create database
2. Start in **test mode** while developing (lock rules down before a public release)
3. Example open rules for testing only:

```json
{
  "rules": {
    "sessions": {
      "$sid": {
        ".read": true,
        ".write": true
      }
    }
  }
}
```

## 3. Google Services plugin
The plugin is already enabled:

```kotlin
alias(libs.plugins.google.services)
```

Sync Gradle, Clean, Rebuild after adding `google-services.json`.

## 4. Play
1. Device A: **Host online (you are the DM)** → enter **DM name only** (no class) → share the session ID
2. Device B: **Join session** → enter that ID → pick a class → wait in the lobby for the DM to admit you
3. Host is authoritative (DM); joiners send actions to the host
4. DM uses in-app DM tools to begin the dungeon / advance rooms / narrate

Never commit real production secrets you care about beyond the usual `google-services.json` client file (it is normal to commit that Android client config, but keep DB rules tight for production).
