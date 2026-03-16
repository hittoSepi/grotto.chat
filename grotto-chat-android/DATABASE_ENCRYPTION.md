# Database Encryption Setup

This guide explains how SQLCipher database encryption works in Grotto Android.

## Overview

Grotto Android encrypts all local data using SQLCipher, which provides:
- **256-bit AES encryption** for the entire database
- **Hardware-backed key storage** via Android Keystore
- **Transparent operation** - no code changes needed for queries

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Your App Code                          │
│              (No changes needed - same Room API)            │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    Room Database Layer                      │
│                      (AndroidX Room)                        │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                   SQLCipher Adapter                         │
│                 (SupportOpenHelperFactory)                  │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    SQLCipher Engine                         │
│                  (256-bit AES encryption)                   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                   Encrypted Database File                   │
│                   (grotto.db - unreadable)                  │
└─────────────────────────────────────────────────────────────┘
```

## Key Management

### Key Generation

```kotlin
// Random 256-bit key (32 bytes)
val keyBytes = ByteArray(32)
SecureRandom().nextBytes(keyBytes)
val hexKey = keyBytes.joinToString("") { "%02x".format(it) }
// Example: "a3f5c8e9..." (64 hex characters)
```

### Key Storage

The encryption key is protected by multiple layers:

1. **Android Keystore** - Hardware-backed when available
2. **AES-256-GCM encryption** - Master key encrypts the database key
3. **EncryptedSharedPreferences** - Stores the encrypted key

```
Database Key (256-bit random)
        ↓
Android Keystore Master Key
        ↓
AES-256-GCM Encryption
        ↓
EncryptedSharedPreferences
```

### Security Properties

| Property | Implementation |
|----------|----------------|
| Key derivation | Hardware-backed (TEE/StrongBox when available) |
| Encryption | AES-256-GCM |
| Key length | 256 bits |
| Storage | Android Keystore + EncryptedSharedPreferences |

## Implementation

### Automatic Encryption

Database encryption is **automatic** and **transparent**:

```kotlin
// In your DAO - no changes needed
@Dao
interface MessageDao {
    @Query("SELECT * FROM messages WHERE channel_id = :channelId")
    suspend fun getMessages(channelId: String): List<MessageEntity>
    
    @Insert
    suspend fun insert(message: MessageEntity)
}

// In your repository - no changes needed
class MessageRepository @Inject constructor(
    private val messageDao: MessageDao,
) {
    suspend fun saveMessage(message: Message) {
        // Automatically encrypted!
        messageDao.insert(message.toEntity())
    }
}
```

### Database Setup

The encryption is configured in `DatabaseModule.kt`:

```kotlin
@Provides
@Singleton
fun provideDatabase(@ApplicationContext context: Context): GrottoDatabase {
    return EncryptedDatabaseFactory.create(
        context = context,
        klass = GrottoDatabase::class.java,
        name = "grotto.db"
    )
}
```

## Verification

### Check Encryption

You can verify the database is encrypted by examining the file:

```bash
# On a rooted device or emulator
adb shell
run-as fi.grotto.android
hexdump -C databases/grotto.db | head -5

# Should show random data (SQLCipher header "SQLite format 3" is encrypted)
# NOT readable SQLite text
```

### Verify Key Storage

Check that the key is properly stored:

```kotlin
val keyManager = DatabaseEncryptionManager(context)
if (keyManager.hasKey()) {
    Log.d("Encryption", "Database key exists")
}
```

## Security Considerations

### Key Backup

**WARNING**: The database key is tied to the device. If you:
- Factory reset the device
- Uninstall the app
- Clear app data

...the key is lost and the database becomes unreadable.

For backup scenarios, consider implementing:
- Encrypted cloud backup with user passphrase
- Key escrow with strong authentication

### Root Access

On rooted devices, the database file is still encrypted. However:
- A determined attacker with root access could extract the key from memory
- Consider root detection and warning users

### Performance

SQLCipher encryption has minimal performance impact:
- Read operations: ~5-10% overhead
- Write operations: ~10-15% overhead
- Negligible for typical chat app usage

## Troubleshooting

### "Database is locked" Error

Usually means another process has the database open. Try:
```kotlin
// Close all database connections
database.close()
// Re-open
```

### "File is encrypted or is not a database" Error

The database file is encrypted but the key is wrong or missing.
Possible causes:
- App data cleared (key lost)
- Database file copied from another device
- Key corruption

**Solution**: The database must be deleted and recreated:
```kotlin
context.deleteDatabase("grotto.db")
```

### Migration from Unencrypted Database

To migrate an existing unencrypted database:

1. Export data from old database
2. Delete old database file
3. Create new encrypted database
4. Import data

```kotlin
// Pseudo-code for migration
val oldDb = Room.databaseBuilder(...).build()
val data = exportData(oldDb)
oldDb.close()
context.deleteDatabase("grotto.db")

val newDb = EncryptedDatabaseFactory.create(...)
importData(newDb, data)
```

## Advanced: Key Rotation

To change the encryption key (requires re-encryption):

```kotlin
val keyManager = DatabaseEncryptionManager(context)
val newKey = keyManager.changeKey()

// Note: This creates a NEW key but doesn't re-encrypt existing data
// Full re-encryption requires exporting and re-importing all data
```

## References

- [SQLCipher Documentation](https://www.zetetic.net/sqlcipher/)
- [AndroidX Security Library](https://developer.android.com/topic/security/data)
- [Android Keystore System](https://developer.android.com/training/articles/keystore)
- [Room Persistence Library](https://developer.android.com/training/data-storage/room)
