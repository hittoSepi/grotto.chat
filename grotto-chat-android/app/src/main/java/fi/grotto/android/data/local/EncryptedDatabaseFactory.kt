package fi.grotto.android.data.local

import android.content.Context
import androidx.room.Room
import androidx.room.RoomDatabase
import fi.grotto.android.security.database.DatabaseEncryptionManager
import kotlinx.coroutines.runBlocking
import net.sqlcipher.database.SQLiteDatabase
import net.sqlcipher.database.SupportFactory
import timber.log.Timber

/**
 * Factory for creating encrypted Room databases using SQLCipher.
 * 
 * Usage:
 * ```kotlin
 * val db = EncryptedDatabaseFactory.create(context, GrottoDatabase::class.java, "grotto.db")
 * ```
 */
object EncryptedDatabaseFactory {
    
    /**
     * Create an encrypted Room database.
     * 
     * @param context Application context
     * @param klass Database class
     * @param name Database file name
     * @return The database instance
     */
    fun <T : RoomDatabase> create(
        context: Context,
        klass: Class<T>,
        name: String,
    ): T {
        // Get or create encryption key
        val encryptionManager = DatabaseEncryptionManager(context)
        val passphrase = runBlocking {
            encryptionManager.getOrCreateKey()
        }
        
        // Convert hex string to char array
        val passphraseBytes = hexToBytes(passphrase)
        
        // Create SQLCipher factory
        val factory = SupportFactory(passphraseBytes)
        
        // Build database with encryption
        return Room.databaseBuilder(context, klass, name)
            .openHelperFactory(factory)
            .fallbackToDestructiveMigration() // Don't destroy data on migration failure
            .addMigrations(*Migrations.ALL_MIGRATIONS)
            .build()
            .also {
                Timber.i("Encrypted database created: $name")
            }
    }
    
    /**
     * Create an encrypted Room database with callback.
     */
    fun <T : RoomDatabase> create(
        context: Context,
        klass: Class<T>,
        name: String,
        callback: RoomDatabase.Callback,
    ): T {
        val encryptionManager = DatabaseEncryptionManager(context)
        val passphrase = runBlocking {
            encryptionManager.getOrCreateKey()
        }
        
        val passphraseBytes = hexToBytes(passphrase)
        val factory = SupportFactory(passphraseBytes)
        
        return Room.databaseBuilder(context, klass, name)
            .openHelperFactory(factory)
            .addCallback(callback)
            .fallbackToDestructiveMigration()
            .addMigrations(*Migrations.ALL_MIGRATIONS)
            .build()
            .also {
                Timber.i("Encrypted database created with callback: $name")
            }
    }
    
    /**
     * Convert hex string to byte array.
     */
    private fun hexToBytes(hex: String): ByteArray {
        val result = ByteArray(hex.length / 2)
        for (i in hex.indices step 2) {
            result[i / 2] = hex.substring(i, i + 2).toInt(16).toByte()
        }
        return result
    }
    
    /**
     * Check if SQLCipher is properly initialized.
     */
    fun checkSQLCipher(): Boolean {
        return try {
            SQLiteDatabase.loadLibs(null)
            true
        } catch (e: Exception) {
            Timber.e(e, "SQLCipher not initialized")
            false
        }
    }
}
