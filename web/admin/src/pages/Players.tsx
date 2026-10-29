// The players of the lobby, in a game or not: remove one, or ban its address.

import { Flex, Link as ChakraLink, Table, Text } from '@chakra-ui/react';
import { useState } from 'react';
import { Link } from 'react-router';

import type { Player } from '../api/types';
import { BanDialog, KickDialog } from '../components/ActionDialogs';
import { FlareButton } from '../components/Flare';
import { Empty, PageHeader, Surface } from '../components/Layout';
import { duration } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { useNow } from '../useNow';

type Dialog = { kind: 'kick'; player: Player } | { kind: 'ban'; address: string };

export function Players() {
  const { players, games } = useLive();
  const now = useNow();
  const [dialog, setDialog] = useState<Dialog | null>(null);
  const t = strings.players;

  return (
    <>
      <PageHeader caption={t.caption} title={t.title} description={t.description} />
      <Surface>
        {players.length === 0 ? (
          <Empty>{t.empty}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.title}>
              <Table.Header>
                <Table.Row>
                  <Table.ColumnHeader>{t.id}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.name}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.address}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.game}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.joined}</Table.ColumnHeader>
                  <Table.ColumnHeader />
                </Table.Row>
              </Table.Header>
              <Table.Body>
                {players.map((player) => {
                  const game = games.find((candidate) => candidate.id === player.gameId);
                  return (
                    <Table.Row key={player.id} data-testid={`player-${player.id}`}>
                      <Table.Cell textStyle="number">{player.id}</Table.Cell>
                      <Table.Cell>{player.name}</Table.Cell>
                      <Table.Cell>{player.address}</Table.Cell>
                      <Table.Cell>
                        {player.gameId === null ? (
                          <Text as="span" color="fg.subtle">
                            {t.inLobby}
                          </Text>
                        ) : (
                          <ChakraLink asChild color="fg" textDecoration="none">
                            <Link to={`/games/${player.gameId}`}>{game?.name ?? `#${player.gameId}`}</Link>
                          </ChakraLink>
                        )}
                      </Table.Cell>
                      <Table.Cell>{duration(now - player.joinedAt)}</Table.Cell>
                      <Table.Cell>
                        <Flex justify="flex-end">
                          <FlareButton
                            size="sm"
                            tone="alert"
                            onClick={() => setDialog({ kind: 'kick', player })}
                            aria-label={`${t.remove} ${player.name}`}
                          >
                            {t.remove}
                          </FlareButton>
                          <FlareButton
                            size="sm"
                            tone="alert"
                            onClick={() => setDialog({ kind: 'ban', address: player.address })}
                            aria-label={`${t.ban} ${player.address}`}
                          >
                            {t.ban}
                          </FlareButton>
                        </Flex>
                      </Table.Cell>
                    </Table.Row>
                  );
                })}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Surface>
      {dialog?.kind === 'kick' && <KickDialog player={dialog.player} onClose={() => setDialog(null)} />}
      {dialog?.kind === 'ban' && <BanDialog address={dialog.address} onClose={() => setDialog(null)} />}
    </>
  );
}
