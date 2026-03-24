package fi.grotto.android.ui.screen.settings

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Dialog for selecting app theme (System/Light/Dark).
 */
@Composable
fun ThemeSelectorDialog(
    currentTheme: String,
    onThemeSelected: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    val options = listOf(
        UserPreferences.THEME_SYSTEM to "System default",
        UserPreferences.THEME_LIGHT to "Light",
        UserPreferences.THEME_DARK to "Dark",
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Select theme") },
        text = {
            Column(modifier = Modifier.selectableGroup()) {
                options.forEach { (value, label) ->
                    ThemeOptionRow(
                        label = label,
                        description = when (value) {
                            UserPreferences.THEME_SYSTEM -> "Follows your phone's theme setting"
                            UserPreferences.THEME_LIGHT -> "Always use light theme"
                            UserPreferences.THEME_DARK -> "Always use dark theme"
                            else -> ""
                        },
                        selected = currentTheme == value,
                        onClick = { onThemeSelected(value) },
                    )
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

@Composable
private fun ThemeOptionRow(
    label: String,
    description: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .selectable(
                selected = selected,
                onClick = onClick,
                role = Role.RadioButton,
            )
            .padding(vertical = GrottoSpacing.sm),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(
            selected = selected,
            onClick = null, // Handled by selectable
        )
        Column(
            modifier = Modifier
                .padding(start = GrottoSpacing.md)
                .weight(1f),
        ) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
            )
            if (description.isNotEmpty()) {
                Text(
                    text = description,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
