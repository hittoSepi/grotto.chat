package fi.grotto.android.ui.screen.settings

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Dialog for selecting font size.
 */
@Composable
fun FontSizeSelectorDialog(
    currentFontScale: Float,
    onFontScaleSelected: (Float) -> Unit,
    onDismiss: () -> Unit,
) {
    val options = listOf(
        FontScaleOption(
            label = "Small",
            scale = UserPreferences.FONT_SCALE_SMALL,
            description = "Smaller text, more content on screen"
        ),
        FontScaleOption(
            label = "Normal",
            scale = UserPreferences.FONT_SCALE_NORMAL,
            description = "Default text size"
        ),
        FontScaleOption(
            label = "Large",
            scale = UserPreferences.FONT_SCALE_LARGE,
            description = "Larger text, easier to read"
        ),
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Font Size") },
        text = {
            Column {
                options.forEach { option ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .selectable(
                                selected = option.scale == currentFontScale,
                                onClick = { onFontScaleSelected(option.scale) }
                            )
                            .padding(vertical = GrottoSpacing.sm),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RadioButton(
                            selected = option.scale == currentFontScale,
                            onClick = { onFontScaleSelected(option.scale) }
                        )
                        Column(modifier = Modifier.padding(start = GrottoSpacing.md)) {
                            Text(
                                text = option.label,
                                style = MaterialTheme.typography.bodyLarge
                            )
                            Text(
                                text = option.description,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Done")
            }
        },
    )
}

private data class FontScaleOption(
    val label: String,
    val scale: Float,
    val description: String,
)
