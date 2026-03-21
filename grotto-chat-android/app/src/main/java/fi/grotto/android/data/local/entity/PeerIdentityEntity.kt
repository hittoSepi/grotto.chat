package fi.grotto.android.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "peer_identities")
data class PeerIdentityEntity(
    @PrimaryKey @ColumnInfo(name = "user_id") val userId: String,
    @ColumnInfo(name = "identity_pub") val identityPub: ByteArray,
    @ColumnInfo(name = "trust_status") val trustStatus: String = "unverified",
    @ColumnInfo(name = "safety_number") val safetyNumber: String? = null,
    @ColumnInfo(name = "public_key") val publicKey: String? = null,  // Base64 encoded for native store
    @ColumnInfo(name = "presence_status") val presenceStatus: String = "offline",  // online, away, offline
    @ColumnInfo(name = "presence_updated_at") val presenceUpdatedAt: Long = 0,  // timestamp of last update
    @ColumnInfo(name = "last_verified_at") val lastVerifiedAt: Long? = null,
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is PeerIdentityEntity) return false
        return userId == other.userId
    }

    override fun hashCode(): Int = userId.hashCode()
}
