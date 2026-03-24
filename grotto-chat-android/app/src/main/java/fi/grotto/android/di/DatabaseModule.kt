package fi.grotto.android.di

import android.content.Context
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import fi.grotto.android.data.local.EncryptedDatabaseFactory
import fi.grotto.android.data.local.GrottoDatabase
import fi.grotto.android.data.local.dao.ChannelDao
import fi.grotto.android.data.local.dao.ChannelMemberDao
import fi.grotto.android.data.local.dao.LinkPreviewDao
import fi.grotto.android.data.local.dao.MessageDao
import fi.grotto.android.data.local.dao.PeerIdentityDao
import fi.grotto.android.crypto.NativeStore
import net.sqlcipher.database.SQLiteDatabase
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object DatabaseModule {

    init {
        // Initialize SQLCipher
        System.loadLibrary("sqlcipher")
    }

    @Provides
    @Singleton
    fun provideDatabase(@ApplicationContext context: Context): GrottoDatabase {
        // Create encrypted database using SQLCipher
        return EncryptedDatabaseFactory.create(
            context = context,
            klass = GrottoDatabase::class.java,
            name = "grotto.db"
        )
    }

    @Provides
    fun provideMessageDao(db: GrottoDatabase): MessageDao = db.messageDao()

    @Provides
    fun provideChannelDao(db: GrottoDatabase): ChannelDao = db.channelDao()

    @Provides
    fun providePeerIdentityDao(db: GrottoDatabase): PeerIdentityDao = db.peerIdentityDao()

    @Provides
    fun provideChannelMemberDao(db: GrottoDatabase): ChannelMemberDao = db.channelMemberDao()

    @Provides
    fun provideLinkPreviewDao(db: GrottoDatabase): LinkPreviewDao = db.linkPreviewDao()

    @Provides
    @Singleton
    fun provideNativeStore(
        @ApplicationContext context: Context,
        db: GrottoDatabase
    ): NativeStore = NativeStore(context, db)
}
