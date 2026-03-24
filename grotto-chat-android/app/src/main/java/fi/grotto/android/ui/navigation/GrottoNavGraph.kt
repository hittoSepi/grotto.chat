package fi.grotto.android.ui.navigation

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import fi.grotto.android.ui.screen.auth.SetupScreen
import fi.grotto.android.ui.screen.channels.ChannelListScreen
import fi.grotto.android.ui.screen.chat.ChatScreen
import fi.grotto.android.ui.screen.settings.SettingsScreen
import fi.grotto.android.ui.screen.verify.SafetyNumberScreen
import fi.grotto.android.ui.screen.voice.CallScreen
import kotlinx.coroutines.flow.first

object GrottoRoutes {
    const val SETUP = "setup"
    const val CHAT = "chat/{channelId}"
    const val CHANNEL_LIST = "channels"
    const val VOICE = "voice/{channelId}"
    const val CALL = "call/{peerId}"
    const val SETTINGS = "settings"
    const val SAFETY_NUMBER = "safety_number/{peerId}"

    fun chat(channelId: String) = "chat/$channelId"
    fun voice(channelId: String) = "voice/$channelId"
    fun call(peerId: String) = "call/$peerId"
    fun safetyNumber(peerId: String) = "safety_number/$peerId"
}

@Composable
fun GrottoNavGraph(
    viewModel: NavigationViewModel = hiltViewModel(),
) {
    val navController = rememberNavController()
    
    // Check if user is registered
    var isRegistered by remember { mutableStateOf<Boolean?>(null) }
    
    LaunchedEffect(Unit) {
        isRegistered = viewModel.isRegistered.first()
    }
    
    // Show loading while checking registration status
    if (isRegistered == null) {
        // Could show a splash screen here
        return
    }
    
    val startDestination = if (isRegistered == true) {
        GrottoRoutes.chat("general")
    } else {
        GrottoRoutes.SETUP
    }

    NavHost(navController = navController, startDestination = startDestination) {

        composable(GrottoRoutes.SETUP) {
            SetupScreen(
                onSetupComplete = {
                    navController.navigate(GrottoRoutes.chat("general")) {
                        popUpTo(GrottoRoutes.SETUP) { inclusive = true }
                    }
                }
            )
        }

        composable(
            route = GrottoRoutes.CHAT,
            arguments = listOf(navArgument("channelId") { type = NavType.StringType })
        ) { backStackEntry ->
            val channelId = backStackEntry.arguments?.getString("channelId") ?: "general"
            ChatScreen(
                channelId = channelId,
                onOpenDrawer = { navController.navigate(GrottoRoutes.CHANNEL_LIST) },
                onNavigateToSettings = { navController.navigate(GrottoRoutes.SETTINGS) },
                onNavigateToVoice = { navController.navigate(GrottoRoutes.voice(it)) },
            )
        }

        composable(GrottoRoutes.CHANNEL_LIST) {
            ChannelListScreen(
                onChannelSelected = { channelId ->
                    navController.navigate(GrottoRoutes.chat(channelId)) {
                        popUpTo(GrottoRoutes.CHANNEL_LIST) { inclusive = true }
                    }
                },
                onNavigateToSettings = { navController.navigate(GrottoRoutes.SETTINGS) },
            )
        }

        composable(
            route = GrottoRoutes.CALL,
            arguments = listOf(navArgument("peerId") { type = NavType.StringType })
        ) { backStackEntry ->
            val peerId = backStackEntry.arguments?.getString("peerId") ?: ""
            CallScreen(
                peerId = peerId,
                onHangup = { navController.popBackStack() },
            )
        }

        composable(GrottoRoutes.SETTINGS) {
            SettingsScreen(onBack = { navController.popBackStack() })
        }

        composable(
            route = GrottoRoutes.SAFETY_NUMBER,
            arguments = listOf(navArgument("peerId") { type = NavType.StringType })
        ) { backStackEntry ->
            val peerId = backStackEntry.arguments?.getString("peerId") ?: ""
            SafetyNumberScreen(
                peerId = peerId,
                onBack = { navController.popBackStack() },
            )
        }
    }
}
