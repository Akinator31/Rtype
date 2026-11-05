// The addresses banned (node 148:533): those banned now, the time left of each, lift one; those lifted or
// expired lately, from the record; and a new ban, its address, duration and reason, the pilots it removes.

import { Field, Flex, Grid, Input, Table, Text } from '@chakra-ui/react';
import { useEffect, useMemo, useState, type FormEvent } from 'react';

import { api } from '../api/client';
import type { Action, Ban } from '../api/types';
import { ban as banAddress, notifyFailure } from '../actions';
import { DurationField, ReasonField, RemovedPilots } from '../components/ActionDialogs';
import { useActions } from '../components/useActions';
import { Button, RowAction } from '../components/Controls';
import { Gauge } from '../components/Gauge';
import { Card, Empty } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { dayTime, duration, fadeByAge, time } from '../format';
import { useLive } from '../live/context';
import { endedBans } from '../records';
import { strings } from '../strings';
import { glow } from '../theme/system';
import { useNow } from '../useNow';
import { byteLength, isAddress, isValidBanDuration, MaxTextBytes } from '../validation';

/** The record read for the bans that ended: enough to reach a few days back. */
const RecordDepth = 200;

function NewBan() {
  const [address, setAddress] = useState('');
  const [seconds, setSeconds] = useState(3600);
  const [other, setOther] = useState(false);
  const [reason, setReason] = useState('');
  const [pending, setPending] = useState(false);
  const t = strings.bans;
  const trimmed = address.trim();
  const validAddress = isAddress(trimmed);
  const ready = validAddress && isValidBanDuration(seconds) && byteLength(reason) <= MaxTextBytes && !pending;

  const clear = () => {
    setAddress('');
    setReason('');
    setSeconds(3600);
    setOther(false);
  };
  const submit = async (event: FormEvent) => {
    event.preventDefault();
    if (!ready) return;
    setPending(true);
    try {
      await banAddress(trimmed, seconds, reason);
      clear();
    } catch (failure) {
      notifyFailure(failure);
    } finally {
      setPending(false);
    }
  };

  return (
    <Card title={t.form} as="form" onSubmit={(event: FormEvent) => void submit(event)} gap="18px">
      <Field.Root invalid={address !== '' && !validAddress}>
        <Field.Label>{t.formAddress}</Field.Label>
        <Input value={address} onChange={(event) => setAddress(event.target.value)} autoComplete="off" />
        {address !== '' && !validAddress && <Field.ErrorText>{t.formAddressInvalid}</Field.ErrorText>}
      </Field.Root>
      <DurationField seconds={seconds} onChange={setSeconds} other={other} onOther={setOther} />
      <ReasonField value={reason} onChange={setReason} />
      {validAddress && <RemovedPilots address={trimmed} />}
      <Flex gap="12px">
        <Button type="submit" variant="danger" disabled={!ready}>
          {t.submit}
        </Button>
        <Button onClick={clear}>{t.cancel}</Button>
      </Flex>
    </Card>
  );
}

export function Bans() {
  const live = useLive();
  const now = useNow();
  const actions = useActions();
  const [bans, setBans] = useState<Ban[]>([]);
  const [record, setRecord] = useState<Action[]>([]);
  const t = strings.bans;
  const latest = live.actions[0]?.id;
  usePageTitle(t.title);

  // The bans change with the actions: asked again after each one
  useEffect(() => {
    let current = true;
    api
      .bans()
      .then((answer) => current && setBans(answer))
      .catch(() => undefined);
    api
      .actions({ limit: RecordDepth })
      .then((answer) => current && setRecord(answer))
      .catch(() => undefined);
    return () => {
      current = false;
    };
  }, [latest]);

  const running = bans.filter((ban) => ban.expiresAt > now);
  const ended = useMemo(() => endedBans(record, bans, now), [record, bans, now]);

  return (
    <Grid templateColumns={{ base: '1fr', xl: '1fr 360px' }} gap="20px" alignItems="start">
      <Flex direction="column" gap="20px" minW={0}>
        <Card title={t.current}>
          {running.length === 0 ? (
            <Empty>{t.empty}</Empty>
          ) : (
            <Table.ScrollArea>
              <Table.Root aria-label={t.current}>
                <Table.Header>
                  <Table.Row>
                    <Table.ColumnHeader>{t.address}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.end}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.left}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.reason}</Table.ColumnHeader>
                    <Table.ColumnHeader />
                  </Table.Row>
                </Table.Header>
                <Table.Body>
                  {running.map((ban) => {
                    const left = ban.expiresAt - now;
                    const whole = Math.max(1, ban.expiresAt - ban.createdAt);
                    return (
                      <Table.Row key={ban.address} h="58px" data-testid={`ban-${ban.address}`}>
                        <Table.Cell textStyle="name">{ban.address}</Table.Cell>
                        <Table.Cell whiteSpace="nowrap">{dayTime(ban.expiresAt)}</Table.Cell>
                        <Table.Cell minW="110px">
                          <Text textStyle="number" fontSize="9px" color="fg">
                            {duration(left)}
                          </Text>
                          <Gauge
                            value={left / whole}
                            label={`${t.left} ${ban.address}`}
                            valueText={duration(left)}
                            width="96px"
                          />
                        </Table.Cell>
                        <Table.Cell color="fg.silver">{ban.reason || strings.none}</Table.Cell>
                        <Table.Cell textAlign="right">
                          <RowAction
                            tone="alert"
                            onClick={() => actions.unban(ban.address)}
                            aria-label={`${t.lift} ${ban.address}`}
                          >
                            {t.lift}
                          </RowAction>
                        </Table.Cell>
                      </Table.Row>
                    );
                  })}
                </Table.Body>
              </Table.Root>
            </Table.ScrollArea>
          )}
        </Card>
        {ended.length > 0 && (
          <Card title={t.recent}>
            <Table.ScrollArea>
              <Table.Root aria-label={t.recent}>
                <Table.Header>
                  <Table.Row>
                    <Table.ColumnHeader>{t.address}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.end}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.reason}</Table.ColumnHeader>
                  </Table.Row>
                </Table.Header>
                <Table.Body>
                  {ended.map((entry) => (
                    <Table.Row key={entry.key} opacity={fadeByAge(now - entry.at)}>
                      <Table.Cell textStyle="number" color="fg" textShadow={glow(0.3)}>
                        {entry.address}
                      </Table.Cell>
                      <Table.Cell>
                        {entry.lifted ? t.liftedAt(time(entry.at)) : t.expiredAt(dayTime(entry.at))}
                      </Table.Cell>
                      <Table.Cell>{entry.reason || strings.none}</Table.Cell>
                    </Table.Row>
                  ))}
                </Table.Body>
              </Table.Root>
            </Table.ScrollArea>
          </Card>
        )}
      </Flex>
      <NewBan />
      {actions.dialogs}
    </Grid>
  );
}
