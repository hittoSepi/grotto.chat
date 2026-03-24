package fi.grotto.android.ui.theme

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * ViewModel for managing app theme state.
 */
@HiltViewModel
class ThemeViewModel @Inject constructor(
    private val userPreferences: UserPreferences,
) : ViewModel() {

    /**
     * Current theme mode as StateFlow.
     * Can be THEME_SYSTEM, THEME_LIGHT, or THEME_DARK.
     */
    val themeMode: StateFlow<String> = userPreferences.themeMode
        .stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = UserPreferences.THEME_SYSTEM
        )

    /**
     * Whether dark theme should be used based on current mode and system setting.
     */
    val isDarkTheme: StateFlow<Boolean?> = userPreferences.themeMode
        .map { mode ->
            when (mode) {
                UserPreferences.THEME_DARK -> true
                UserPreferences.THEME_LIGHT -> false
                else -> null // null means follow system
            }
        }
        .stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = null
        )

    /**
     * Set the theme mode.
     * @param mode One of THEME_SYSTEM, THEME_LIGHT, THEME_DARK
     */
    fun setThemeMode(mode: String) {
        viewModelScope.launch {
            userPreferences.setThemeMode(mode)
        }
    }

    /**
     * Get human-readable name for a theme mode.
     */
    fun getThemeModeName(mode: String): String = when (mode) {
        UserPreferences.THEME_SYSTEM -> "System default"
        UserPreferences.THEME_LIGHT -> "Light"
        UserPreferences.THEME_DARK -> "Dark"
        else -> "System default"
    }

    /**
     * Get icon description for a theme mode.
     */
    fun getThemeModeDescription(mode: String): String = when (mode) {
        UserPreferences.THEME_SYSTEM -> "Follows your phone's theme setting"
        UserPreferences.THEME_LIGHT -> "Always use light theme"
        UserPreferences.THEME_DARK -> "Always use dark theme"
        else -> "Follows your phone's theme setting"
    }
}
