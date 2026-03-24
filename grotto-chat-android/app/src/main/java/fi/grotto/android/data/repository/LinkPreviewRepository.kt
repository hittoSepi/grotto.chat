package fi.grotto.android.data.repository

import fi.grotto.android.data.local.dao.LinkPreviewDao
import fi.grotto.android.data.local.entity.LinkPreviewEntity
import fi.grotto.android.data.local.entity.isValid
import fi.grotto.android.domain.model.LinkPreview
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import timber.log.Timber
import java.io.IOException
import java.net.URL
import java.util.concurrent.TimeUnit
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class LinkPreviewRepository @Inject constructor(
    private val linkPreviewDao: LinkPreviewDao,
) {
    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .followRedirects(true)
        .followSslRedirects(true)
        .build()

    /**
     * Get link preview for a URL. Returns cached result if valid,
     * otherwise fetches fresh OG metadata.
     */
    suspend fun getLinkPreview(url: String): LinkPreview? = withContext(Dispatchers.IO) {
        // Check cache first
        val cached = linkPreviewDao.getByUrl(url)
        if (cached?.isValid() == true) {
            Timber.d("Using cached link preview for $url")
            return@withContext cached.toDomainModel()
        }

        // Fetch fresh data
        val fetched = fetchOgMetadata(url)
        if (fetched != null) {
            // Save to cache
            linkPreviewDao.insert(fetched)
            Timber.d("Fetched and cached link preview for $url")
            fetched.toDomainModel()
        } else {
            // Save error state to avoid repeated failed fetches
            linkPreviewDao.insert(
                LinkPreviewEntity(
                    url = url,
                    fetchStatus = "error",
                    cachedAt = System.currentTimeMillis(),
                )
            )
            null
        }
    }

    /**
     * Fetch OG metadata from URL. Returns null on failure.
     */
    private suspend fun fetchOgMetadata(url: String): LinkPreviewEntity? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url(url)
                .header("User-Agent", "Mozilla/5.0 (compatible; Grotto-Android/0.1)")
                .build()

            client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    Timber.w("Failed to fetch $url: ${response.code}")
                    return@withContext null
                }

                val contentType = response.header("Content-Type") ?: ""
                if (!contentType.contains("text/html", ignoreCase = true)) {
                    // Not HTML, can't extract OG tags
                    return@withContext null
                }

                val html = response.body?.string() ?: return@withContext null
                parseOpenGraph(url, html)
            }
        } catch (e: IOException) {
            Timber.e(e, "Network error fetching $url")
            null
        } catch (e: Exception) {
            Timber.e(e, "Error fetching OG metadata for $url")
            null
        }
    }

    /**
     * Parse Open Graph metadata from HTML.
     */
    private fun parseOpenGraph(url: String, html: String): LinkPreviewEntity {
        val title = extractMetaTag(html, "og:title")
            ?: extractMetaTag(html, "twitter:title")
            ?: extractTitleTag(html)
            ?: URL(url).host

        val description = extractMetaTag(html, "og:description")
            ?: extractMetaTag(html, "twitter:description")
            ?: extractMetaTag(html, "description")

        val imageUrl = extractMetaTag(html, "og:image")
            ?: extractMetaTag(html, "twitter:image")

        val siteName = extractMetaTag(html, "og:site_name")

        return LinkPreviewEntity(
            url = url,
            title = title?.take(200),
            description = description?.take(500),
            imageUrl = imageUrl?.take(500),
            siteName = siteName?.take(100),
            fetchStatus = "success",
            cachedAt = System.currentTimeMillis(),
        )
    }

    private fun extractMetaTag(html: String, property: String): String? {
        // Match: <meta property="og:title" content="..."> or <meta name="og:title" content="...">
        val regex = """<meta\s+(?:property|name)=["']$property["']\s+content=["']([^"']+)["']""".toRegex(RegexOption.IGNORE_CASE)
        return regex.find(html)?.groupValues?.get(1)?.trim()
    }

    private fun extractTitleTag(html: String): String? {
        val regex = """<title>([^<]+)</title>""".toRegex(RegexOption.IGNORE_CASE)
        return regex.find(html)?.groupValues?.get(1)?.trim()
    }

    /**
     * Clear expired cache entries.
     */
    suspend fun clearExpiredCache(): Int = withContext(Dispatchers.IO) {
        val cutoff = System.currentTimeMillis() - (7 * 24 * 60 * 60 * 1000L) // 7 days
        val count = linkPreviewDao.deleteOlderThan(cutoff)
        Timber.d("Cleared $count expired link preview cache entries")
        count
    }

    private fun LinkPreviewEntity.toDomainModel(): LinkPreview {
        return LinkPreview(
            url = url,
            title = title,
            description = description,
            imageUrl = imageUrl,
        )
    }
}
