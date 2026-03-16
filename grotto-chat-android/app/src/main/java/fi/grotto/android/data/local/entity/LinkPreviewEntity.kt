package fi.grotto.android.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

/**
 * Cached Open Graph metadata for URLs.
 * TTL-based cache expiration - old entries are refreshed on demand.
 */
@Entity(
    tableName = "link_previews",
    indices = [
        Index("url", unique = true),
        Index("cached_at"),
    ]
)
data class LinkPreviewEntity(
    @PrimaryKey @ColumnInfo(name = "url") val url: String,
    @ColumnInfo(name = "title") val title: String? = null,
    @ColumnInfo(name = "description") val description: String? = null,
    @ColumnInfo(name = "image_url") val imageUrl: String? = null,
    @ColumnInfo(name = "site_name") val siteName: String? = null,
    @ColumnInfo(name = "cached_at") val cachedAt: Long = System.currentTimeMillis(),
    @ColumnInfo(name = "fetch_status") val fetchStatus: String = "pending", // pending, success, error
)

/**
 * Cache TTL in milliseconds (24 hours)
 */
const val LINK_PREVIEW_CACHE_TTL_MS = 24 * 60 * 60 * 1000L

fun LinkPreviewEntity.isExpired(): Boolean {
    return System.currentTimeMillis() - cachedAt > LINK_PREVIEW_CACHE_TTL_MS
}

fun LinkPreviewEntity.isValid(): Boolean {
    return fetchStatus == "success" && !isExpired()
}
