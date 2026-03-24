package fi.grotto.android.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import fi.grotto.android.ui.theme.GrottoTheme
import fi.grotto.android.ui.theme.nickColor

@Composable
fun UserAvatar(
    userId: String,
    size: Dp = 40.dp,
    modifier: Modifier = Modifier,
) {
    val color = nickColor(userId, GrottoTheme.semanticColors.nickPalette)

    Box(
        modifier = modifier
            .size(size)
            .clip(CircleShape)
            .background(color.copy(alpha = 0.3f)),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = userId.take(1).uppercase(),
            style = MaterialTheme.typography.titleMedium,
            color = color,
        )
    }
}
