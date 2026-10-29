import { useState } from 'react';

import { notifyFailure } from '../actions';
import { api } from '../api/client';
import { strings } from '../strings';
import { isValidMessage } from '../validation';
import { notify } from './notify';

/** Sends a message to a game, or to everyone: the text emptied once sent, a toast says to how many. */
export function useSendMessage(target: { id: number; name: string } | null) {
  const [text, setText] = useState('');
  const [pending, setPending] = useState(false);
  const send = async () => {
    if (!isValidMessage(text) || pending) return;
    setPending(true);
    try {
      const sent = await api.message(text.trim(), target?.id ?? null);
      setText('');
      notify(
        'bonus',
        strings.notify.sent,
        strings.notify.sentDetail(
          sent.recipients,
          target === null ? strings.messages.everyone : `${target.name} #${target.id}`,
        ),
      );
    } catch (failure) {
      notifyFailure(failure);
    } finally {
      setPending(false);
    }
  };
  return { text, setText, send, pending, ready: isValidMessage(text) && !pending };
}
