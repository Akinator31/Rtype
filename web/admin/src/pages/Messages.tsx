// The messages of the server (node 149:263): to the whole lobby or to a game, from a template or not, seen as
// the players will see it; and those sent, from the record.

import { Box, Flex, Grid, Table, Text } from '@chakra-ui/react';
import { useEffect, useState, type FormEvent } from 'react';
import { useSearchParams } from 'react-router';

import { api } from '../api/client';
import type { Action } from '../api/types';
import stars from '../assets/backgrounds/stars.svg';
import { Button, Chip } from '../components/Controls';
import { ToneIcon } from '../components/EventFeed';
import { Card, Empty } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { MessageField } from '../components/MessageField';
import { useSendMessage } from '../components/useSendMessage';
import { fadeByAge, time } from '../format';
import { useLive } from '../live/context';
import { setPreferences, usePreferences } from '../preferences';
import { merged } from '../records';
import { strings } from '../strings';
import { chromeGradient, flareGlow, flareLine, glow } from '../theme/system';
import { useNow } from '../useNow';
import { isValidMessage } from '../validation';

/** The record read for the messages sent. */
const RecordDepth = 200;

/** The message as the game shows it: its title between the flares, its text under it. */
function Preview({ text }: { text: string }) {
  const t = strings.messages;
  return (
    <Box
      position="relative"
      h="120px"
      borderWidth="1px"
      borderColor="border"
      borderRadius="l2"
      bg="bg"
      bgImage={`url(${stars})`}
      bgSize="cover"
      overflow="hidden"
      aria-label={t.preview}
      role="img"
    >
      <Flex direction="column" align="center" gap="8px" pt="28px" px="16px">
        <Flex align="center" gap="8px">
          <ToneIcon tone="info" />
          <Text
            fontFamily="heading"
            fontSize="11px"
            letterSpacing="2.4px"
            textTransform="uppercase"
            bgImage={chromeGradient}
            bgClip="text"
            color="transparent"
            filter="drop-shadow(0 0 8px rgba(255,255,255,0.6))"
          >
            {t.previewTitle}
          </Text>
        </Flex>
        <Box h="1px" w="300px" maxW="100%" bgImage={flareLine} boxShadow={flareGlow} />
        <Text fontSize="10px" color="fg.muted" textAlign="center" maxW="100%" truncate>
          {text.trim() === '' ? strings.none : text}
        </Text>
      </Flex>
    </Box>
  );
}

function Compose({ target, setTarget }: { target: number | null; setTarget: (id: number | null) => void }) {
  const { games, players } = useLive();
  const { templates } = usePreferences();
  const game = games.find((candidate) => candidate.id === target) ?? null;
  const message = useSendMessage(game === null ? null : { id: game.id, name: game.name });
  const recipients = game === null ? players.length : game.players.length;
  const t = strings.messages;

  const submit = (event: FormEvent) => {
    event.preventDefault();
    void message.send();
  };

  return (
    <Card title={t.compose} as="form" onSubmit={submit} gap="16px">
      <Flex direction="column" gap="12px">
        <Text textStyle="caption" color="fg.muted">
          {t.recipients}
        </Text>
        <Flex gap="8px" wrap="wrap" role="group" aria-label={t.recipients}>
          <Chip pressed={game === null} onClick={() => setTarget(null)}>
            {t.everyoneRecipient(players.length)}
          </Chip>
          {games.map((candidate) => (
            <Chip
              key={candidate.id}
              pressed={game?.id === candidate.id}
              onClick={() => setTarget(candidate.id)}
            >
              {t.gameRecipient(candidate.name, candidate.id, candidate.players.length)}
            </Chip>
          ))}
        </Flex>
      </Flex>
      <MessageField value={message.text} onChange={message.setText} />
      <Flex direction="column" gap="12px">
        <Text textStyle="caption" color="fg.muted">
          {t.templates}
        </Text>
        <Flex gap="8px" wrap="wrap" role="group" aria-label={t.templates}>
          {templates.map((template) => (
            <Chip
              key={template}
              pressed={message.text === template}
              onClick={() => message.setText(template)}
              onContextMenu={(event) => {
                event.preventDefault();
                setPreferences({ templates: templates.filter((other) => other !== template) });
              }}
              title={t.removeTemplate(template)}
            >
              {template}
            </Chip>
          ))}
          <Chip
            pressed={false}
            tone="muted"
            disabled={!isValidMessage(message.text) || templates.includes(message.text.trim())}
            onClick={() => setPreferences({ templates: [...templates, message.text.trim()] })}
          >
            {t.saveTemplate}
          </Chip>
        </Flex>
      </Flex>
      <Flex direction="column" gap="12px">
        <Text textStyle="caption" color="fg.muted">
          {t.preview}
        </Text>
        <Preview text={message.text} />
      </Flex>
      <Flex>
        <Button type="submit" variant="primary" disabled={!message.ready}>
          {t.send(recipients)}
        </Button>
      </Flex>
    </Card>
  );
}

function Sent({ messages }: { messages: Action[] }) {
  const { games } = useLive();
  const now = useNow(60_000);
  const t = strings.messages;
  return (
    <Card title={t.sent} gap="12px">
      {messages.length === 0 ? (
        <Empty>{t.sentEmpty}</Empty>
      ) : (
        <Table.ScrollArea>
          <Table.Root aria-label={t.sent}>
            <Table.Header>
              <Table.Row>
                <Table.ColumnHeader>{t.time}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.target}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.textColumn}</Table.ColumnHeader>
                <Table.ColumnHeader textAlign="right">{t.received}</Table.ColumnHeader>
              </Table.Row>
            </Table.Header>
            <Table.Body>
              {messages.map((action) => {
                const id = /^game:(\d+)$/.exec(action.target)?.[1];
                const name = games.find((game) => String(game.id) === id)?.name;
                return (
                  <Table.Row key={action.id} opacity={fadeByAge(now - action.at)}>
                    <Table.Cell>{time(action.at)}</Table.Cell>
                    <Table.Cell textStyle="name" textShadow={glow(0.3)}>
                      {id === undefined ? t.lobby : `${name ?? 'game'} #${id}`}
                    </Table.Cell>
                    <Table.Cell color="fg.silver">
                      {typeof action.details.text === 'string' ? action.details.text : strings.none}
                    </Table.Cell>
                    <Table.Cell textAlign="right" textStyle="number" color="fg">
                      {typeof action.details.recipients === 'number'
                        ? action.details.recipients
                        : strings.none}
                    </Table.Cell>
                  </Table.Row>
                );
              })}
            </Table.Body>
          </Table.Root>
        </Table.ScrollArea>
      )}
    </Card>
  );
}

export function Messages() {
  const live = useLive();
  const [params, setParams] = useSearchParams();
  const [record, setRecord] = useState<Action[]>([]);
  const t = strings.messages;
  usePageTitle(t.title);
  const to = params.get('to');
  const target = to === null || Number.isNaN(Number(to)) ? null : Number(to);

  useEffect(() => {
    let current = true;
    api
      .actions({ limit: RecordDepth })
      .then((answer) => current && setRecord(answer))
      .catch(() => undefined);
    return () => {
      current = false;
    };
  }, []);

  const messages = merged(live.actions, record).filter((action) => action.action === 'message');
  return (
    <Grid templateColumns={{ base: '1fr', xl: '1fr 1fr' }} gap="20px" alignItems="stretch">
      <Compose
        target={target}
        setTarget={(id) => setParams(id === null ? {} : { to: String(id) }, { replace: true })}
      />
      <Sent messages={messages} />
    </Grid>
  );
}
