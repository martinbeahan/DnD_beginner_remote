# Firebase setup (online multiplayer)

Online Host / Join needs **Firebase Realtime Database**. The app builds without it; online buttons will say Firebase isn’t configured until you finish these steps.

## 1. Create a Firebase project
1. Open [Firebase Console](https://console.firebase.google.com/)
2. Add a project (any name)
3. Add an **Android** app with package name: `com.fintrack.dndbeginnerremote`
4. Download `google-services.json` and place it at:
   `app/google-services.json`

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

## 3. Enable the Google Services plugin
In `app/build.gradle.kts`, uncomment:

```kotlin
alias(libs.plugins.google.services)
```

Sync Gradle, Clean, Rebuild.

## 4. Play
1. Device A: **Host Online (DM)** → pick name/class → share the session ID
2. Device B: **Join Session** → enter that ID → pick class
3. Host is authoritative (DM); joiners send actions to the host

Never commit real production secrets you care about beyond the usual `google-services.json` client file (it is normal to commit that Android client config, but keep DB rules tight for production).
