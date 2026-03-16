package fi.grotto.android.ui.components

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.LockOpen
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import fi.grotto.android.domain.model.TrustStatus
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun EncryptionBadge(
    trustStatus: TrustStatus,
    modifier: Modifier = Modifier,
) {
    val semantic = GrottoTheme.semanticColors
    val (icon, color, label) = when (trustStatus) {
        TrustStatus.VERIFIED -> Triple(Icons.Default.Lock, semantic.encryptionOk, "Verified")
        TrustStatus.UNVERIFIED -> Triple(Icons.Default.LockOpen, semantic.encryptionUnverified, "Unverified")
        TrustStatus.WARNING -> Triple(Icons.Default.Warning, semantic.encryptionWarning, "Key changed")
    }

    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, contentDescription = label, tint = color, modifier = Modifier.size(16.dp))
        Spacer(Modifier.width(GrottoSpacing.xs))
        Text(label, style = MaterialTheme.typography.labelSmall, color = color)
    }
}
