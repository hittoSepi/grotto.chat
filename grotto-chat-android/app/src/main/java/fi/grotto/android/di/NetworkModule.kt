package fi.grotto.android.di

import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import fi.grotto.android.data.remote.FrameCodec
import fi.grotto.android.data.remote.GrottoSocket
import fi.grotto.android.data.remote.ReconnectPolicy
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object NetworkModule {

    @Provides
    @Singleton
    fun provideFrameCodec(): FrameCodec = FrameCodec()

    @Provides
    @Singleton
    fun provideReconnectPolicy(): ReconnectPolicy = ReconnectPolicy()

    @Provides
    @Singleton
    fun provideGrottoSocket(
        frameCodec: FrameCodec,
        reconnectPolicy: ReconnectPolicy,
    ): GrottoSocket = GrottoSocket(frameCodec, reconnectPolicy)
}
