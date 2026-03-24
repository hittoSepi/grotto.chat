package fi.grotto.android.data.local

import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase

/**
 * Database migrations for GrottoDatabase.
 */
object Migrations {
    
    /**
     * Migration from version 1 to 2:
     * - Adds public_key column to peer_identities table for native store compatibility
     */
    val MIGRATION_1_2 = object : Migration(1, 2) {
        override fun migrate(db: SupportSQLiteDatabase) {
            // Add public_key column for native store Base64 encoded keys
            db.execSQL("ALTER TABLE peer_identities ADD COLUMN public_key TEXT DEFAULT NULL")
        }
    }
    
    /**
     * Migration from version 2 to 3:
     * - Adds topic column to channels table
     */
    val MIGRATION_2_3 = object : Migration(2, 3) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL("ALTER TABLE channels ADD COLUMN topic TEXT DEFAULT NULL")
        }
    }

    /**
     * Migration from version 3 to 4:
     * - Adds presence_status and presence_updated_at columns to peer_identities table
     */
    val MIGRATION_3_4 = object : Migration(3, 4) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL("ALTER TABLE peer_identities ADD COLUMN presence_status TEXT DEFAULT 'offline'")
            db.execSQL("ALTER TABLE peer_identities ADD COLUMN presence_updated_at INTEGER DEFAULT 0")
        }
    }

    /**
     * Migration from version 4 to 5:
     * - Adds channel_members table for tracking channel membership
     */
    val MIGRATION_4_5 = object : Migration(4, 5) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL("""
                CREATE TABLE IF NOT EXISTS channel_members (
                    channel_id TEXT NOT NULL,
                    user_id TEXT NOT NULL,
                    nickname TEXT NOT NULL,
                    role TEXT NOT NULL DEFAULT 'regular',
                    joined_at INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY(channel_id, user_id)
                )
            """)
            db.execSQL("CREATE INDEX IF NOT EXISTS index_channel_members_channel_id ON channel_members(channel_id)")
            db.execSQL("CREATE INDEX IF NOT EXISTS index_channel_members_user_id ON channel_members(user_id)")
        }
    }

    /**
     * Migration from version 5 to 6:
     * - Adds link_previews table for caching Open Graph metadata
     */
    val MIGRATION_5_6 = object : Migration(5, 6) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL("""
                CREATE TABLE IF NOT EXISTS link_previews (
                    url TEXT PRIMARY KEY NOT NULL,
                    title TEXT,
                    description TEXT,
                    image_url TEXT,
                    site_name TEXT,
                    cached_at INTEGER NOT NULL DEFAULT 0,
                    fetch_status TEXT NOT NULL DEFAULT 'pending'
                )
            """)
            db.execSQL("CREATE INDEX IF NOT EXISTS index_link_previews_url ON link_previews(url)")
            db.execSQL("CREATE INDEX IF NOT EXISTS index_link_previews_cached_at ON link_previews(cached_at)")
        }
    }

    /**
     * Migration from version 6 to 7:
     * - Adds last_verified_at column to peer_identities table
     */
    val MIGRATION_6_7 = object : Migration(6, 7) {
        override fun migrate(db: SupportSQLiteDatabase) {
            db.execSQL("ALTER TABLE peer_identities ADD COLUMN last_verified_at INTEGER DEFAULT NULL")
        }
    }

    /**
     * All migrations in order.
     */
    val ALL_MIGRATIONS: Array<Migration> = arrayOf(
        MIGRATION_1_2,
        MIGRATION_2_3,
        MIGRATION_3_4,
        MIGRATION_4_5,
        MIGRATION_5_6,
        MIGRATION_6_7,
    )
}
