// Opening a session without one (node 146:34) and after one expired (node 153:1245): the token of
// admin-url.txt, pasted or read from the file itself, traded for a session by the master.

import { Field, Flex, Image, Input, Text } from '@chakra-ui/react';
import { useRef, useState, type ChangeEvent, type FormEvent } from 'react';

import { session } from '../api/client';
import logo from '../assets/images/logo.png';
import { strings } from '../strings';
import { chromeGradient } from '../theme/system';
import { findToken } from '../validation';
import { DialogFrame } from './ConfirmDialog';
import { Button } from './Controls';

/** The token field, and what reads it from admin-url.txt. */
function useTokenField() {
  const [text, setText] = useState('');
  const [error, setError] = useState<string | null>(null);
  const file = useRef<HTMLInputElement>(null);
  const token = findToken(text);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    if (token === null) {
      setError(strings.session.tokenInvalid);
      return;
    }
    session.open(token);
  };

  const read = async (event: ChangeEvent<HTMLInputElement>) => {
    const chosen = event.target.files?.[0];
    event.target.value = '';
    if (chosen === undefined) return;
    const found = findToken(await chosen.text());
    if (found === null) {
      setError(strings.session.fileInvalid);
      return;
    }
    setText(found);
    setError(null);
  };

  const field = (
    <Field.Root invalid={error !== null}>
      <Field.Label>{strings.session.token}</Field.Label>
      <Input
        value={text}
        onChange={(event) => {
          setText(event.target.value);
          setError(null);
        }}
        autoComplete="off"
        spellCheck={false}
        fontFamily="body"
        placeholder={strings.session.tokenHint}
      />
      {error !== null && <Field.ErrorText>{error}</Field.ErrorText>}
    </Field.Root>
  );
  const picker = (
    <input
      ref={file}
      type="file"
      accept=".txt,text/plain"
      hidden
      data-testid="admin-url-file"
      onChange={(event) => void read(event)}
    />
  );
  return { field, picker, submit, pick: () => file.current?.click() };
}

/** The page of a panel opened without a session: the logo, then the token of admin-url.txt. */
export function Login() {
  const { field, picker, submit, pick } = useTokenField();
  return (
    <Flex direction="column" align="center" minH="100vh" px="16px" py="48px" gap="24px" data-testid="login">
      <Image
        src={logo}
        alt={strings.brand}
        w={{ base: '280px', md: '400px' }}
        aspectRatio="16 / 9"
        objectFit="cover"
      />
      <Text
        as="h1"
        fontFamily="heading"
        fontSize="13px"
        letterSpacing="4px"
        textTransform="uppercase"
        bgImage={chromeGradient}
        bgClip="text"
        color="transparent"
        filter="drop-shadow(0 0 8px rgba(255,255,255,0.45))"
      >
        {strings.panel}
      </Text>
      <Flex
        as="form"
        onSubmit={submit}
        layerStyle="card"
        bgImage="linear-gradient(180deg, rgba(10,11,13,0.9) 0%, rgba(0,0,0,0.9) 100%)"
        direction="column"
        gap="18px"
        px="24px"
        py="24px"
        w="100%"
        maxW="460px"
        aria-label={strings.session.loginTitle}
      >
        <Text as="h2" textStyle="cardTitle">
          {strings.session.loginTitle}
        </Text>
        <Text textStyle="body" color="fg.muted" lineHeight="18px">
          {strings.session.loginBody}
        </Text>
        {field}
        <Flex gap="12px" wrap="wrap">
          <Button type="submit" variant="primary">
            {strings.session.open}
          </Button>
          <Button variant="secondary" onClick={pick}>
            {strings.session.readFile}
          </Button>
          {picker}
        </Flex>
      </Flex>
    </Flex>
  );
}

/** The window over the page when the session expired: the page stays, a new token reopens it. */
export function SessionExpired() {
  const { field, picker, submit, pick } = useTokenField();
  return (
    <DialogFrame
      title={strings.session.expiredTitle}
      tone="info"
      onSubmit={submit}
      actions={
        <>
          <Button variant="secondary" onClick={pick}>
            {strings.session.readFile}
          </Button>
          <Button type="submit" variant="primary">
            {strings.session.reopen}
          </Button>
          {picker}
        </>
      }
    >
      <Text textStyle="body" color="fg.muted" lineHeight="18px">
        {strings.session.expiredBody}
      </Text>
      {field}
    </DialogFrame>
  );
}
