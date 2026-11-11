# Guides

Step-by-step guides for common tasks in the client and the server. They assume the project builds, see
[Build](../getting-started/build.md).

| Guide | What you will do |
|---|---|
| [Write a system](write-a-system.md) | Add a component and a system to the ECS, and test it. |
| [Draw a sprite](draw-a-sprite.md) | Add an image to the assets, load it and show it with an entity. |
| [Add a menu](add-a-menu.md) | Build a menu with widgets, open it and close it. |
| [Add a player action](add-a-player-action.md) | Add a new action, bind it to keys and buttons, and react to it. |
| [Add a network message](add-a-network-message.md) | Specify a message, encode and decode it, and test it. |
| [Configure enemies and weapons](configure-enemies-and-weapons.md) | Add a projectile, a weapon, a ship, an enemy and a wave with Lua files, and check them. |
| [Script the behavior of an enemy](script-enemy-behaviors.md) | Write in Lua when an enemy moves, where it goes and when it fires, and try it. |
| [Manage the server key](generate-the-server-keys.md) | Keep the key the server makes at its first start, give its fingerprint to the players, replace it. |
| [Run a server](run-a-server.md) | Check the configuration, open the ports, start the server, open its panel and stop it. |
| [Configure a server](configure-a-server.md) | Give a server its settings in one file, its secrets in files of their own, and check them. |
| [Set up accounts](set-up-accounts.md) | Give the players of a server their accounts: the mail server their codes are sent through, what they can do, and the data kept. |
| [Administer a server](administer-a-server.md) | Open the panel of a server, watch its games and players live, end a game, message, remove, ban. |
| [Deploy with Docker](deploy-with-docker.md) | Run the server from its image in Dokploy, publish its ports, keep its volume, back up its database; then with its accounts, its secrets as files, and the panel on its domain. |
| [Use an external database](use-an-external-database.md) | Give a server a PostgreSQL database hosted elsewhere, without its password on the command line. |
| [Play online](play-online.md) | Start a server, connect to it, trust its key, create and join a game. |
| [Play in VR](play-in-vr.md) | Start a runtime, connect a headset, start the client in it, and find why it shows nothing. |

For how the systems used here work, see [Scenes and interface](../architecture/scenes-and-interface.md),
[Rendering](../architecture/rendering.md), [Input](../architecture/input.md) and
[Networking](../architecture/networking.md).
