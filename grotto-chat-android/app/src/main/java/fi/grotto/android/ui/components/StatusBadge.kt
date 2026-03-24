package fi.grotto.android.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import fi.grotto.android.domain.model.PresenceStatus
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun StatusBadge(
    status: PresenceStatus,
    modifier: Modifier = Modifier,
) {
    val semantic = GrottoTheme.semanticColors
    val color = when (status) {
        PresenceStatus.ONLINE -> semantic.statusOnline
        PresenceStatus.AWAY -> semantic.statusAway
        PresenceStatus.OFFLINE -> semantic.statusOffline
    }

    Box(
        modifier = modifier
            .size(GrottoSpacing.statusDotSize)
            .clip(CircleShape)
            .background(color),
    )
}
