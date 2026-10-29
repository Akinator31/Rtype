// The text of a message to the players (nodes 147:712, 149:400): one line, its bytes counted once it nears the
// most, checked before it is sent, as the master checks it after.

import { Field, Input } from '@chakra-ui/react';
import type { Ref } from 'react';

import { strings } from '../strings';
import { byteLength, isValidMessage, MaxTextBytes, NearLimit } from '../validation';

export function MessageField({
  value,
  onChange,
  inputRef,
}: {
  value: string;
  onChange: (value: string) => void;
  inputRef?: Ref<HTMLInputElement>;
}) {
  const used = byteLength(value);
  const invalid = value !== '' && !isValidMessage(value);
  return (
    <Field.Root invalid={invalid}>
      <Field.Label>{strings.messages.text}</Field.Label>
      <Input
        ref={inputRef}
        value={value}
        onChange={(event) => onChange(event.target.value)}
        placeholder={strings.messages.placeholder}
        autoComplete="off"
      />
      {invalid ? (
        <Field.ErrorText>{strings.messages.bytesInvalid(used, MaxTextBytes)}</Field.ErrorText>
      ) : (
        used > NearLimit && <Field.HelperText>{strings.messages.bytes(used, MaxTextBytes)}</Field.HelperText>
      )}
    </Field.Root>
  );
}
