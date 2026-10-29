// The window of the design over a veiled page (nodes 152:800, 153:1245): its icon and title,
// the flare under them, what it does, its fields, then a note and its buttons. Red for what removes, ends or
// bans; white for the rest. The dialog that confirms an action stays open with the error of the master when
// the action is refused.

import { Box, Dialog, Portal, Text } from '@chakra-ui/react';
import { useState, type FormEvent, type ReactNode } from 'react';

import { ApiError } from '../api/client';
import { strings } from '../strings';
import {
  alertFlareGlow,
  alertFlareLine,
  alertGlow,
  chromeGradient,
  flareGlow,
  flareLine,
} from '../theme/system';
import { Button } from './Controls';
import { ToneIcon } from './EventFeed';

export interface DialogFrameProps {
  title: string;
  tone?: 'info' | 'alert';
  /** Esc and the backdrop close it; without, it stays until it is done. */
  onClose?: () => void;
  onSubmit: (event: FormEvent) => void;
  children: ReactNode;
  /** The note, then the buttons, at its foot. */
  note?: ReactNode;
  actions: ReactNode;
  role?: 'dialog' | 'alertdialog';
}

export function DialogFrame({
  title,
  tone = 'alert',
  onClose,
  onSubmit,
  children,
  note,
  actions,
  role = 'alertdialog',
}: DialogFrameProps) {
  const alert = tone === 'alert';
  return (
    <Dialog.Root
      open
      role={role}
      placement="center"
      closeOnEscape={onClose !== undefined}
      closeOnInteractOutside={onClose !== undefined}
      onOpenChange={(details) => {
        if (!details.open) onClose?.();
      }}
    >
      <Portal>
        <Dialog.Backdrop />
        <Dialog.Positioner px="16px">
          <Dialog.Content
            as="form"
            onSubmit={onSubmit}
            maxW="540px"
            w="100%"
            borderColor={alert ? 'rgba(232,114,110,0.45)' : 'rgba(255,255,255,0.3)'}
            boxShadow={alert ? '0 0 48px 0 rgba(232,114,110,0.14)' : '0 0 48px 0 rgba(255,255,255,0.12)'}
            data-tone={tone}
          >
            <Dialog.Header display="flex" alignItems="center" gap="12px">
              <ToneIcon tone={alert ? 'alert' : 'info'} />
              <Dialog.Title
                fontFamily="heading"
                fontWeight="normal"
                fontSize="15px"
                lineHeight="normal"
                letterSpacing="2.4px"
                textTransform="uppercase"
                {...(alert
                  ? { color: 'alert', textShadow: alertGlow(0.45) }
                  : {
                      bgImage: chromeGradient,
                      bgClip: 'text',
                      color: 'transparent',
                      filter: 'drop-shadow(0 0 10px rgba(255,255,255,0.35))',
                    })}
              >
                {title}
              </Dialog.Title>
            </Dialog.Header>
            <Box
              h="1px"
              flexShrink={0}
              bgImage={alert ? alertFlareLine : flareLine}
              boxShadow={alert ? alertFlareGlow : flareGlow}
            />
            <Dialog.Body display="flex" flexDirection="column" gap="18px">
              {children}
            </Dialog.Body>
            <Dialog.Footer display="flex" alignItems="center" gap="12px" flexWrap="wrap">
              {note !== undefined && (
                <Text textStyle="hint" color="fg.subtle">
                  {note}
                </Text>
              )}
              <Box flex="1 0 0" />
              {actions}
            </Dialog.Footer>
          </Dialog.Content>
        </Dialog.Positioner>
      </Portal>
    </Dialog.Root>
  );
}

export interface ConfirmDialogProps {
  title: string;
  body: string;
  confirmLabel: string;
  tone?: 'info' | 'alert';
  /** Whether the fields are right: the confirmation waits for them. */
  ready?: boolean;
  note?: ReactNode;
  children?: ReactNode;
  /** Does the action; the dialog closes once it succeeded. */
  onConfirm: () => Promise<void>;
  onClose: () => void;
}

export function ConfirmDialog({
  title,
  body,
  confirmLabel,
  tone = 'alert',
  ready = true,
  note,
  children,
  onConfirm,
  onClose,
}: ConfirmDialogProps) {
  const [pending, setPending] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    if (!ready || pending) return;
    setPending(true);
    setError(null);
    try {
      await onConfirm();
      onClose();
    } catch (failure) {
      setError(failure instanceof ApiError ? failure.message : String(failure));
      setPending(false);
    }
  };

  return (
    <DialogFrame
      title={title}
      tone={tone}
      onClose={pending ? undefined : onClose}
      onSubmit={(event) => void submit(event)}
      note={note}
      actions={
        <>
          <Button variant="secondary" onClick={onClose} disabled={pending}>
            {strings.dialog.cancel}
          </Button>
          <Button
            type="submit"
            variant={tone === 'alert' ? 'danger' : 'primary'}
            disabled={!ready || pending}
          >
            {confirmLabel}
          </Button>
        </>
      }
    >
      <Text textStyle="body" lineHeight="18px" color="fg.muted">
        {body}
      </Text>
      {children}
      {error !== null && (
        <Text role="alert" textStyle="hint" color="alert">
          {error}
        </Text>
      )}
    </DialogFrame>
  );
}
