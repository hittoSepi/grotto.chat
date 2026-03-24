package fi.grotto.android.ui.theme

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * ViewModel for managing font scale state.
 */
@HiltViewModel
class FontScaleViewModel @Inject constructor(
    private val userPreferences: UserPreferences,
) : ViewModel() {

    /**
     * Current font scale as StateFlow.
     * Values: 0.85f (Small), 1.0f (Normal), 1.15f (Large)
     */
    val fontScale: StateFlow<Float> = userPreferences.fontScale
        .stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = UserPreferences.FONT_SCALE_NORMAL
        )

    /**
     * Set the font scale.
     * @param scale One of FONT_SCALE_SMALL, FONT_SCALE_NORMAL, FONT_SCALE_LARGE
     */
    fun setFontScale(scale: Float) {
        viewModelScope.launch {
            userPreferences.setFontScale(scale)
        }
    }

    /**
     * Get human-readable name for a font scale.
     */
    fun getFontScaleName(scale: Float): String = when (scale) {
        UserPreferences.FONT_SCALE_SMALL -> "Small"
        UserPreferences.FONT_SCALE_NORMAL -> "Normal"
        UserPreferences.FONT_SCALE_LARGE -> "Large"
        else -> "Normal"
    }
}
