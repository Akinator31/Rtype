// The actions of the administration (docs/protocol/admin-api.md, 4), each behind its window: remove a player,
// ban an address, lift a ban, end a game. The server records each one; a toast tells the host.

import { Box, Field, Flex, Input, Text } from '@chakra-ui/react';
import { useState } from 'react';

import { ban, end, kick, type GameRef, type PlayerRef } from '../actions';
import { api } from '../api/client';
import { dayTime, shortDuration } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { alertGlow, glow } from '../theme/system';
import { useNow } from '../useNow';
import { byteLength, isAddress, isValidBanDuration, MaxTextBytes, NearLimit } from '../validation';
import { ConfirmDialog } from './ConfirmDialog';
import { Chip } from './Controls';
import { notify } from './notify';

/** The field of an optional reason; its bytes counted once it nears the most the server takes. */
export function ReasonField({ value, onChange }: { value: string; onChange: (value: string) => void }) {
  const used = byteLength(value);
  const tooLong = used > MaxTextBytes;
  return (
    <Field.Root invalid={tooLong}>
      <Field.Label>{strings.dialog.reason}</Field.Label>
      <Input
        value={value}
        onChange={(event) => onChange(event.target.value)}
        placeholder={strings.dialog.reasonHint}
        autoComplete="off"
      />
      {tooLong ? (
        <Field.ErrorText>{strings.dialog.bytes(used)}</Field.ErrorText>
      ) : (
        used > NearLimit && <Field.HelperText>{strings.dialog.bytes(used)}</Field.HelperText>
      )}
    </Field.Root>
  );
}

const fits = (reason: string) => byteLength(reason) <= MaxTextBytes;

export function KickDialog({ player, onClose }: { player: PlayerRef; onClose: () => void }) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.kickTitle}
      body={strings.dialog.kickBody(`${player.name} #${player.id}`)}
      confirmLabel={strings.dialog.kickConfirm}
      ready={fits(reason)}
      onClose={onClose}
      onConfirm={() => kick(player, reason)}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

export function EndGameDialog({ game, onClose }: { game: GameRef; onClose: () => void }) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.endTitle}
      body={strings.dialog.endBody(`${game.name} #${game.id}`, game.players)}
      confirmLabel={strings.dialog.endConfirm}
      ready={fits(reason)}
      onClose={onClose}
      onConfirm={() => end(game, reason)}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

/** The durations of a ban to choose from, then any other in seconds. */
export function DurationField({
  seconds,
  onChange,
  other,
  onOther,
}: {
  seconds: number;
  onChange: (seconds: number) => void;
  other: boolean;
  onOther: (other: boolean) => void;
}) {
  const valid = isValidBanDuration(seconds);
  return (
    <>
      <Field.Root>
        <Field.Label>{strings.dialog.duration}</Field.Label>
        <Flex gap="8px" wrap="wrap" role="group" aria-label={strings.dialog.duration}>
          {strings.dialog.durations.map((choice) => (
            <Chip
              key={choice.seconds}
              pressed={!other && seconds === choice.seconds}
              onClick={() => {
                onOther(false);
                onChange(choice.seconds);
              }}
            >
              {choice.label}
            </Chip>
          ))}
          <Chip pressed={other} tone="muted" onClick={() => onOther(true)}>
            {strings.dialog.other}
          </Chip>
        </Flex>
      </Field.Root>
      {other && (
        <Field.Root invalid={!valid}>
          <Field.Label>{strings.bans.formSeconds}</Field.Label>
          <Input
            inputMode="numeric"
            value={Number.isNaN(seconds) ? '' : String(seconds)}
            onChange={(event) =>
              onChange(event.target.value === '' ? Number.NaN : Number(event.target.value))
            }
            autoComplete="off"
          />
          {!valid && <Field.ErrorText>{strings.bans.formSecondsInvalid}</Field.ErrorText>}
        </Field.Root>
      )}
    </>
  );
}

/** The pilots a ban of this address removes, red: who they are, and their game. */
export function RemovedPilots({ address }: { address: string }) {
  const { players, games } = useLive();
  const removed = players.filter((player) => player.address === address);
  if (removed.length === 0) {
    return (
      <Text textStyle="hint" color="fg.subtle">
        {strings.dialog.banNoneRemoved}
      </Text>
    );
  }
  return (
    <Flex
      direction="column"
      gap="10px"
      px="16px"
      py="14px"
      borderWidth="1px"
      borderColor="rgba(232,114,110,0.35)"
      borderRadius="l2"
      bg="rgba(232,114,110,0.05)"
      data-testid="removed-pilots"
    >
      <Text
        fontFamily="heading"
        fontSize="10px"
        letterSpacing="2px"
        textTransform="uppercase"
        color="alert"
        textShadow={alertGlow(0.45)}
      >
        {strings.dialog.banRemoved(removed.length)}
      </Text>
      {removed.map((player) => {
        const game = games.find((candidate) => candidate.id === player.gameId);
        return (
          <Flex key={player.id} align="center" gap="10px">
            <Text textStyle="name" textShadow={glow(0.35)}>
              {player.name} #{player.id}
            </Text>
            <Box flex="1 0 0" />
            <Text textStyle="hint" color="fg.muted" textTransform="uppercase">
              {player.gameId === null ? strings.players.lobby : `${game?.name ?? ''} #${player.gameId}`}
            </Text>
          </Flex>
        );
      })}
    </Flex>
  );
}

export function BanDialog({ address: initial, onClose }: { address: string; onClose: () => void }) {
  const [address, setAddress] = useState(initial);
  const [reason, setReason] = useState('');
  const [seconds, setSeconds] = useState(3600);
  const [other, setOther] = useState(false);
  const now = useNow();
  const validAddress = isAddress(address.trim());
  const valid = validAddress && isValidBanDuration(seconds) && fits(reason);
  return (
    <ConfirmDialog
      title={strings.dialog.banTitle}
      body={strings.dialog.banBody}
      confirmLabel={strings.dialog.banConfirm(isValidBanDuration(seconds) ? shortDuration(seconds) : '')}
      ready={valid}
      note={isValidBanDuration(seconds) ? strings.dialog.banEnds(dayTime(now + seconds * 1000)) : undefined}
      onClose={onClose}
      onConfirm={() => ban(address.trim(), seconds, reason)}
    >
      <Field.Root invalid={!validAddress}>
        <Field.Label>{strings.dialog.banAddress}</Field.Label>
        <Input value={address} onChange={(event) => setAddress(event.target.value)} autoComplete="off" />
        {!validAddress && <Field.ErrorText>{strings.bans.formAddressInvalid}</Field.ErrorText>}
      </Field.Root>
      <DurationField seconds={seconds} onChange={setSeconds} other={other} onOther={setOther} />
      <ReasonField value={reason} onChange={setReason} />
      {validAddress && <RemovedPilots address={address.trim()} />}
    </ConfirmDialog>
  );
}

export function UnbanDialog({ address, onClose }: { address: string; onClose: () => void }) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.unbanTitle}
      body={strings.dialog.unbanBody(address)}
      confirmLabel={strings.dialog.unbanConfirm}
      tone="info"
      ready={fits(reason)}
      onClose={onClose}
      onConfirm={async () => {
        await api.unban(address, reason.trim());
        notify('bonus', strings.notify.unbanned, strings.notify.unbannedDetail(address));
      }}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}
