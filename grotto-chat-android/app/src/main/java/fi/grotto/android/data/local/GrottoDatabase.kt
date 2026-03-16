package fi.grotto.android.data.local

import androidx.room.Database
import androidx.room.RoomDatabase
import fi.grotto.android.data.local.dao.ChannelDao
import fi.grotto.android.data.local.dao.ChannelMemberDao
import fi.grotto.android.data.local.dao.LinkPreviewDao
import fi.grotto.android.data.local.dao.MessageDao
import fi.grotto.android.data.local.dao.PeerIdentityDao
import fi.grotto.android.data.local.entity.ChannelEntity
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.data.local.entity.LinkPreviewEntity
import fi.grotto.android.data.local.entity.MessageEntity
import fi.grotto.android.data.local.entity.PeerIdentityEntity

@Database(
    entities = [
        MessageEntity::class,
        ChannelEntity::class,
        PeerIdentityEntity::class,
        ChannelMemberEntity::class,
        LinkPreviewEntity::class,
    ],
    version = 6,
    exportSchema = true,
)
abstract class GrottoDatabase : RoomDatabase() {
    abstract fun messageDao(): MessageDao
    abstract fun channelDao(): ChannelDao
    abstract fun peerIdentityDao(): PeerIdentityDao
    abstract fun channelMemberDao(): ChannelMemberDao
    abstract fun linkPreviewDao(): LinkPreviewDao
}
