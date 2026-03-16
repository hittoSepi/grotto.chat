package fi.grotto.android.di

import android.content.ContentResolver
import android.content.Context
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import fi.grotto.android.crypto.NativeStore
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.data.remote.GrottoConnectionManager
import fi.grotto.android.data.remote.GrottoSocket
import fi.grotto.android.data.repository.ChannelMemberRepository
import fi.grotto.android.data.repository.KeyRepository
import fi.grotto.android.data.repository.MessageRepository
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun provideApplicationContext(@ApplicationContext context: Context): Context = context

    @Provides
    @Singleton
    fun provideContentResolver(@ApplicationContext context: Context): ContentResolver =
        context.contentResolver

    @Provides
    @Singleton
    fun provideGrottoConnectionManager(
        socket: GrottoSocket,
        userPreferences: UserPreferences,
        messageRepository: MessageRepository,
        keyRepository: KeyRepository,
        channelMemberRepository: ChannelMemberRepository,
        nativeStore: NativeStore,
    ): GrottoConnectionManager =
        GrottoConnectionManager(socket, userPreferences, messageRepository, keyRepository, channelMemberRepository, nativeStore)
}
