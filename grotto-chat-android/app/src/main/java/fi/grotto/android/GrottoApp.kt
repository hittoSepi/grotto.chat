package fi.grotto.android

import android.app.Application
import dagger.hilt.android.HiltAndroidApp
import fi.grotto.android.data.repository.FileRepository
import timber.log.Timber
import javax.inject.Inject

@HiltAndroidApp
class GrottoApp : Application() {

    @Inject
    lateinit var fileRepository: FileRepository

    override fun onCreate() {
        super.onCreate()
        if (BuildConfig.DEBUG) {
            Timber.plant(Timber.DebugTree())
        }
        
        // Initialize file transfer handlers
        fileRepository.initialize()
        
        Timber.d("GrottoApp initialized")
    }
}
