package fi.grotto.android.data.local.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import fi.grotto.android.data.local.entity.LinkPreviewEntity

@Dao
interface LinkPreviewDao {

    @Query("SELECT * FROM link_previews WHERE url = :url LIMIT 1")
    suspend fun getByUrl(url: String): LinkPreviewEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(preview: LinkPreviewEntity)

    @Query("UPDATE link_previews SET title = :title, description = :description, image_url = :imageUrl, site_name = :siteName, cached_at = :cachedAt, fetch_status = :status WHERE url = :url")
    suspend fun update(url: String, title: String?, description: String?, imageUrl: String?, siteName: String?, cachedAt: Long, status: String)

    @Query("DELETE FROM link_previews WHERE url = :url")
    suspend fun delete(url: String)

    @Query("DELETE FROM link_previews WHERE cached_at < :olderThan")
    suspend fun deleteOlderThan(olderThan: Long): Int

    @Query("SELECT COUNT(*) FROM link_previews")
    suspend fun count(): Int

    @Query("DELETE FROM link_previews")
    suspend fun deleteAll()
}
