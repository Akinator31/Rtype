// Opening a session without one (P1′ of the Série 14, after node 146:34) and after one expired (node 153:1245):
// an administrator account of the server, Discord first, then its password, when the master has accounts, the
// token of admin-url.txt folded under it; or, without accounts, the token alone, pasted or read from the file
// itself, traded for a session by the master.

import { Box, Field, Flex, Image, Input, Text } from '@chakra-ui/react';
import { useRef, useState, type ChangeEvent, type FormEvent, type ReactNode } from 'react';

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
  const discord = options?.discord === true;
  // Discord first, as in the game: the password below it, a secondary way
  const discordButton = discord ? (
    <Button variant="primary" w="100%" onClick={() => session.discord()}>
      <DiscordLogo />
      {t.discord}
    </Button>
  ) : null;
  const signIn = (
    <Button
      type="submit"
      variant={discord ? 'secondary' : 'primary'}
      disabled={login.trim() === '' || password === '' || pending}
    >
      {t.signIn}
    </Button>
  );
  const buttons = (
    <>
      {signIn}
      {discord && (
        <Button variant="secondary" onClick={() => session.discord()}>
          {t.discord}
        </Button>
      )}
    </>
  );
  return { fields, buttons, discordButton, signIn, submit };
}

/** The logo of Discord, in the colour of the text of its button: silver, never the blue of Discord. */
function DiscordLogo() {
  return (
    <svg viewBox="0 0 24 24" width="16" height="16" aria-hidden fill="currentColor" style={{ flexShrink: 0 }}>
      <path d="M20.317 4.37a19.79 19.79 0 0 0-4.885-1.515.074.074 0 0 0-.079.037c-.21.375-.444.865-.608 1.25a18.27 18.27 0 0 0-5.487 0 12.64 12.64 0 0 0-.617-1.25.077.077 0 0 0-.079-.037A19.74 19.74 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.057a.082.082 0 0 0 .031.057 19.9 19.9 0 0 0 5.993 3.03.078.078 0 0 0 .084-.028c.462-.63.874-1.295 1.226-1.994a.076.076 0 0 0-.041-.106 13.1 13.1 0 0 1-1.872-.892.077.077 0 0 1-.008-.128c.126-.094.252-.192.372-.291a.074.074 0 0 1 .078-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.009c.12.1.246.198.373.292a.077.077 0 0 1-.007.128 12.3 12.3 0 0 1-1.873.891.077.077 0 0 0-.041.107c.36.698.772 1.363 1.225 1.993a.076.076 0 0 0 .084.029 19.84 19.84 0 0 0 6.002-3.03.077.077 0 0 0 .032-.054c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03zM8.02 15.331c-1.183 0-2.157-1.086-2.157-2.419 0-1.333.956-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.332-.956 2.418-2.157 2.418zm7.975 0c-1.183 0-2.157-1.086-2.157-2.419 0-1.333.955-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.332-.946 2.418-2.157 2.418z" />
    </svg>
  );
}

/** An "or" between two ways of signing in. */
function Or() {
  return (
    <Flex align="center" gap="12px" aria-hidden>
      <Box flex="1" h="1px" bg="border" />
      <Text textStyle="caption" color="fg.subtle">
        {strings.session.or}
      </Text>
      <Box flex="1" h="1px" bg="border" />
    </Flex>
  );
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

/** The card of an administrator account: Discord, or its login and password; the local token folded at its
 * foot. */
function AccountCard({ options, local }: { options: SignInOptions; local: ReactNode }) {
  const { fields, discordButton, signIn, submit } = useAccountFields(options);
  return (
    <Flex {...cardStyle} direction="column">
      <Flex
        as="form"
        onSubmit={(event: FormEvent) => void submit(event)}
        direction="column"
        gap="18px"
        aria-label={strings.session.signInTitle}
      >
        <Text as="h2" textStyle="cardTitle">
          {strings.session.signInTitle}
        </Text>
        <Text textStyle="body" color="fg.muted" lineHeight="18px">
          {strings.session.signInBody}
        </Text>
        {discordButton !== null && (
          <>
            {discordButton}
            <Or />
          </>
        )}
        {fields}
        <Flex gap="12px" wrap="wrap">
          {signIn}
        </Flex>
      </Flex>
      <Box h="1px" bgImage="linear-gradient(90deg, transparent, rgba(255,255,255,0.85), transparent)" />
      {local}
    </Flex>
  );
}

/** The local token under an account card: what it is, and the form once asked for. */
function LocalAccess({ form }: { form: ReactNode }) {
  const [open, setOpen] = useState(false);
  const t = strings.session;
  return (
    <Flex direction="column" gap="10px" data-testid="local-access">
      <Text textStyle="caption" color="fg.muted">
        {t.localTitle}
      </Text>
      <Text textStyle="body" color="fg.subtle" lineHeight="18px">
        {t.localBody}
      </Text>
      {open ? (
        form
      ) : (
        <Box>
          <Button variant="secondary" onClick={() => setOpen(true)}>
            {t.useToken}
          </Button>
        </Box>
      )}
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
      {options?.password === true ? (
        <AccountCard
          options={options}
          local={
            <LocalAccess
              form={<TokenForm field={field} picker={picker} submit={submit} pick={pick} plain />}
            />
          }
        />
      ) : (
        <TokenForm field={field} picker={picker} submit={submit} pick={pick} />
      )}
    </Flex>
  );
}

/** The form of the token of admin-url.txt: a card of its own, or plain inside the card of an account. */
function TokenForm({
  field,
  picker,
  submit,
  pick,
  plain = false,
}: {
  field: ReactNode;
  picker: ReactNode;
  submit: (event: FormEvent) => void;
  pick: () => void;
  plain?: boolean;
}) {
  return (
    <Flex
      as="form"
      onSubmit={submit}
      {...(plain ? { direction: 'column', gap: '18px' } : cardStyle)}
      aria-label={strings.session.loginTitle}
    >
      {!plain && (
        <>
          <Text as="h2" textStyle="cardTitle">
            {strings.session.loginTitle}
          </Text>
          <Text textStyle="body" color="fg.muted" lineHeight="18px">
            {strings.session.loginBody}
          </Text>
        </>
      )}
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
