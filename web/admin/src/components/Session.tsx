// Opening a session without one (node 146:34) and after one expired (node 153:1245): an administrator account
// of the server, its password or Discord, when the master has accounts; or the token of admin-url.txt, pasted
// or read from the file itself, traded for a session by the master.

import { Field, Flex, Image, Input, Text } from '@chakra-ui/react';
import { useRef, useState, type ChangeEvent, type FormEvent } from 'react';

import { ApiError, api, session } from '../api/client';
import type { SignInOptions } from '../api/types';
import logo from '../assets/images/logo.png';
import { strings } from '../strings';
import { chromeGradient } from '../theme/system';
import { findToken } from '../validation';
import { DialogFrame } from './ConfirmDialog';
import { useSignInOptions } from './useSignInOptions';
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

/** The fields of an account and its password, and the way to Discord. */
function useAccountFields(options: SignInOptions | null) {
  const [login, setLogin] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState(false);
  const t = strings.session;

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    if (login.trim() === '' || password === '' || pending) return;
    setPending(true);
    setError(null);
    try {
      await api.signIn(login.trim(), password);
      session.reload();
    } catch (failure) {
      setError(failure instanceof ApiError ? failure.message : String(failure));
      setPassword('');
      setPending(false);
    }
  };

  const fields = (
    <>
      <Field.Root>
        <Field.Label>{t.login}</Field.Label>
        <Input
          value={login}
          onChange={(event) => setLogin(event.target.value)}
          autoComplete="username"
          spellCheck={false}
          fontFamily="body"
        />
      </Field.Root>
      <Field.Root invalid={error !== null}>
        <Field.Label>{t.password}</Field.Label>
        <Input
          type="password"
          value={password}
          onChange={(event) => setPassword(event.target.value)}
          autoComplete="current-password"
          fontFamily="body"
        />
        {error !== null && <Field.ErrorText>{error}</Field.ErrorText>}
      </Field.Root>
    </>
  );
  const buttons = (
    <>
      <Button type="submit" variant="primary" disabled={login.trim() === '' || password === '' || pending}>
        {t.signIn}
      </Button>
      {options?.discord === true && (
        <Button variant="secondary" onClick={() => session.discord()}>
          {t.discord}
        </Button>
      )}
    </>
  );
  return { fields, buttons, submit };
}

const cardStyle = {
  layerStyle: 'card',
  bgImage: 'linear-gradient(180deg, rgba(10,11,13,0.9) 0%, rgba(0,0,0,0.9) 100%)',
  direction: 'column',
  gap: '18px',
  px: '24px',
  py: '24px',
  w: '100%',
  maxW: '460px',
} as const;

/** The card of an administrator account: its login and password, or Discord. */
function AccountCard({ options }: { options: SignInOptions }) {
  const { fields, buttons, submit } = useAccountFields(options);
  return (
    <Flex
      as="form"
      onSubmit={(event: FormEvent) => void submit(event)}
      {...cardStyle}
      aria-label={strings.session.signInTitle}
    >
      <Text as="h2" textStyle="cardTitle">
        {strings.session.signInTitle}
      </Text>
      <Text textStyle="body" color="fg.muted" lineHeight="18px">
        {strings.session.signInBody}
      </Text>
      {fields}
      <Flex gap="12px" wrap="wrap">
        {buttons}
      </Flex>
    </Flex>
  );
}

/** The page of a panel opened without a session: the logo, then an account if the master has them, and the
 * token of admin-url.txt. */
export function Login() {
  const { field, picker, submit, pick } = useTokenField();
  const options = useSignInOptions();
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
      {options?.password === true && (
        <>
          <AccountCard options={options} />
          <Text textStyle="hint" color="fg.subtle">
            {strings.session.or}
          </Text>
        </>
      )}
      <Flex as="form" onSubmit={submit} {...cardStyle} aria-label={strings.session.loginTitle}>
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

/** The window over the page when the session expired: the page stays, an account or a new token reopens it. */
export function SessionExpired() {
  const options = useSignInOptions();
  if (options?.password === true) return <AccountExpired options={options} />;
  return <TokenExpired />;
}

function AccountExpired({ options }: { options: SignInOptions }) {
  const { fields, buttons, submit } = useAccountFields(options);
  return (
    <DialogFrame
      title={strings.session.expiredTitle}
      tone="info"
      onSubmit={(event) => void submit(event)}
      actions={buttons}
    >
      <Text textStyle="body" color="fg.muted" lineHeight="18px">
        {strings.session.expiredAccountBody}
      </Text>
      {fields}
    </DialogFrame>
  );
}

function TokenExpired() {
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
