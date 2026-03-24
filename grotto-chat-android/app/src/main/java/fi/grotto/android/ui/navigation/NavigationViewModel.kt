package fi.grotto.android.ui.navigation

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject

@HiltViewModel
class NavigationViewModel @Inject constructor(
    userPreferences: UserPreferences,
) : ViewModel() {
    val isRegistered: Flow<Boolean> = userPreferences.isRegistered
}
