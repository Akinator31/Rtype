# The code of the client is a library of its own, so that the tests can use it, like the server's:
#   App/                 what the program puts together: its configuration, its systems and its scenes
#   Commands/, Console/  the console and its commands
#   Game/                a game: its session, offline or online, and its scenes (ADR 0015)
#   Loading/             the loading screen at startup, and the loading of the assets of the scenes
#   Lore/                the story of the game, scrolling away towards the horizon
#   MainMenu/            the home screen and its submenus
#   Online/              playing online: the lobby of a server, and its scenes
#   Quests/              the quests of the player, woken by the events of the game
#   ShipSelector/        the hangar: the catalog of ships and the scene where the player picks one
#   ShipModel            the ship models and their animations, flown by the menu and by the game
add_library(r-type_client_core STATIC
    src/App/Configuration.cpp
    src/App/Scenes.cpp
    src/App/Systems.cpp
    src/Commands/BossCommand.cpp
    src/Commands/Commands.cpp
    src/Commands/FpsCommand.cpp
    src/Commands/HitboxesCommand.cpp
    src/Commands/QuitCommand.cpp
    src/Commands/SceneCommand.cpp
    src/Commands/Switch.cpp
    src/Commands/ToastCommand.cpp
    src/Console/Console.cpp
    src/Console/ConsoleScene.cpp
    src/Console/ConsoleSystem.cpp
    src/Controls/KeyCaptureSystem.cpp
    src/Game/BossLooks.cpp
    src/Game/DamageLooks.cpp
    src/Game/DamageNumberSystem.cpp
    src/Game/DeathScreen.cpp
    src/Game/DeathScreenSystem.cpp
    src/Game/EntityLook.cpp
    src/Game/EntitySounds.cpp
    src/Game/GameRecorder.cpp
    src/Game/GameScenes.cpp
    src/Game/GameSessionSystem.cpp
    src/Game/GameView.cpp
    src/Game/HealthBarLooks.cpp
    src/Game/HealthBarSystem.cpp
    src/Game/Hud.cpp
    src/Game/HudSystem.cpp
    src/Game/ImpactLooks.cpp
    src/Game/ImpactSystem.cpp
    src/Game/InputClock.cpp
    src/Game/Interpolation.cpp
    src/Game/LocalSession.cpp
    src/Game/Prediction.cpp
    src/Game/RemoteSession.cpp
    src/Game/ResultsScreen.cpp
    src/Game/ResultsSystem.cpp
    src/Game/RescueCameraSystem.cpp
    src/Game/ShipLooks.cpp
    src/Game/ShotPrediction.cpp
    src/Game/ShotLooks.cpp
    src/Loading/AssetPreload.cpp
    src/Loading/LoadingScene.cpp
    src/Loading/LoadingSystem.cpp
    src/Loading/SceneAssetLoader.cpp
    src/Loading/SceneLoadStatus.cpp
    src/Lore/CrawlSystem.cpp
    src/Lore/LoreLayout.cpp
    src/Lore/LoreScene.cpp
    src/Lore/LoreText.cpp
    src/MainMenu/Capitals.cpp
    src/MainMenu/FlightSystem.cpp
    src/MainMenu/IntroSystem.cpp
    src/MainMenu/Logo.cpp
    src/MainMenu/LogoFlareSystem.cpp
    src/MainMenu/MainMenu.cpp
    src/MainMenu/Planet.cpp
    src/MainMenu/ControlsMenu.cpp
    src/MainMenu/SettingsMenu.cpp
    src/MainMenu/Ships.cpp
    src/MainMenu/Station.cpp
    src/MainMenu/Submenus.cpp
    src/MainMenu/UiShowcase.cpp
    src/Online/AccountScenes.cpp
    src/Online/FormErrors.cpp
    src/Online/KnownProfiles.cpp
    src/Online/KnownServers.cpp
    src/Online/LobbyClient.cpp
    src/Online/OnlineScenes.cpp
    src/Online/OnlineScreen.cpp
    src/Online/SessionTokens.cpp
    src/Settings/AuraSettings.cpp
    src/Settings/ControlSettings.cpp
    src/Settings/DisplaySettings.cpp
    src/Settings/PlayerSettings.cpp
    src/Quests/EventQuest.cpp
    src/Quests/Quests.cpp
    src/Quests/StatQuest.cpp
    src/ShipModel.cpp
    src/ShipSelector/ShipCatalog.cpp
    src/ShipSelector/ShipSelection.cpp
    src/ShipSelector/ShipSelector.cpp
    src/ShipSelector/ShipSelectorSystem.cpp
)
target_include_directories(r-type_client_core PUBLIC src)
target_link_libraries(r-type_client_core
    PUBLIC rtype::audio rtype::discord rtype::ecs rtype::engine rtype::game rtype::input rtype::lobby rtype::localization
           rtype::log rtype::network rtype::physics rtype::render
    PRIVATE rtype_options)

# Without the Discord Social SDK for this platform (libs/discord), the core above only, for the tests
if(NOT TARGET rtype::discord_sdk)
  return()
endif()

add_executable(r-type_client
    src/main.cpp
)
# The Discord Social SDK in the executable only (libs/discord): the tests of the client never load it
target_link_libraries(r-type_client PRIVATE rtype_options r-type_client_core rtype::discord_sdk)
rtype_add_discord_runtime(r-type_client client)

# The client finds its assets next to its executable, both in the build tree and once installed.
# A custom target runs on every build, so that a modified asset is copied even if the code did not change.
add_custom_target(r-type_client_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:r-type_client>/assets
    COMMENT "Copying client assets"
)
add_dependencies(r-type_client r-type_client_assets)

install(TARGETS r-type_client RUNTIME DESTINATION . COMPONENT client)
install(DIRECTORY assets/ DESTINATION assets COMPONENT client PATTERN ".gitkeep" EXCLUDE)
