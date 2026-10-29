// The dialog that confirms every action: what it does, its fields, then Cancel or Confirm. It stays open
// with the error of the server when the action is refused.

import { Dialog, Flex, Portal, Text } from '@chakra-ui/react';
import { useState, type FormEvent, type ReactNode } from 'react';

import { ApiError } from '../api/client';
import { strings } from '../strings';
import { FlareButton } from './Flare';

export interface ConfirmDialogProps {
  title: string;
  body: string;
  confirmLabel?: string;
  tone?: 'light' | 'alert';
  /** Whether the fields are right: Confirm waits for them. */
  ready?: boolean;
  children?: ReactNode;
  /** Does the action; the dialog closes once it succeeded. */
  onConfirm: () => Promise<void>;
  onClose: () => void;
}

export function ConfirmDialog({
  title,
  body,
  confirmLabel = strings.dialog.confirm,
  tone = 'alert',
  ready = true,
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
    <Dialog.Root
      open
      role="alertdialog"
      placement="center"
      onOpenChange={(details) => {
        if (!details.open && !pending) onClose();
      }}
    >
      <Portal>
        <Dialog.Backdrop />
        <Dialog.Positioner px="16px">
          <Dialog.Content as="form" onSubmit={(event: FormEvent) => void submit(event)} maxW="480px">
            <Dialog.Header>
              <Dialog.Title fontWeight="normal">{title}</Dialog.Title>
            </Dialog.Header>
            <Dialog.Body display="flex" flexDirection="column" gap="20px">
              <Text>{body}</Text>
              {children}
              {error !== null && (
                <Text role="alert" color="alert">
                  {error}
                </Text>
              )}
            </Dialog.Body>
            <Dialog.Footer>
              <Flex gap="8px" justify="flex-end" w="100%">
                <FlareButton onClick={onClose} disabled={pending}>
                  {strings.dialog.cancel}
                </FlareButton>
                <FlareButton type="submit" tone={tone} disabled={!ready || pending}>
                  {confirmLabel}
                </FlareButton>
              </Flex>
            </Dialog.Footer>
          </Dialog.Content>
        </Dialog.Positioner>
      </Portal>
    </Dialog.Root>
  );
}
