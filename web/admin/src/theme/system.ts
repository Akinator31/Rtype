// The theme of the panel, after the Figma section "Panel d'administration" (node 144:28), direction "Éclat
// stellaire": black and silver, light as the only accent, red for alerts. Its components (row 0: the navigation
// entry, the action button, the state badge, the form field, the feedback toast) and its screens set the values
// here once; the pages and the components use these tokens, text styles and recipes, never a value of their own.
// The reference: https://www.figma.com/design/RrrH0ic4ttdyDoc3nRnWBr/Untitled?node-id=144-28

import { createSystem, defaultConfig, defineConfig, defineRecipe } from '@chakra-ui/react';
import { dialogAnatomy, fieldAnatomy, tableAnatomy } from '@chakra-ui/react/anatomy';

/** The silver of the titles, clipped to their text. */
export const chromeGradient =
  'linear-gradient(180deg, #ffffff 0%, #c3c8cd 42%, #5e6369 52%, #a3a8ae 64%, #f4f6f8 100%)';

/** The flare: a 1 px horizontal line, transparent → white 85 % → transparent, with its glow. */
export const flareLine =
  'linear-gradient(90deg, rgba(255,255,255,0) 0%, rgba(255,255,255,0.85) 50%, rgba(255,255,255,0) 100%)';
export const alertFlareLine =
  'linear-gradient(90deg, rgba(232,114,110,0) 0%, rgba(232,114,110,0.85) 50%, rgba(232,114,110,0) 100%)';
export const silverFlareLine =
  'linear-gradient(90deg, rgba(195,200,205,0) 0%, rgba(195,200,205,0.85) 50%, rgba(195,200,205,0) 100%)';
export const flareGlow = '0 0 6px 0 rgba(255,255,255,0.7)';
export const alertFlareGlow = '0 0 6px 0 rgba(232,114,110,0.7)';
export const silverFlareGlow = '0 0 6px 0 rgba(195,200,205,0.6)';

/** The glows of a lit text: white, or red for an alert. */
export const glow = (strength: number) => `0 0 8px rgba(255,255,255,${strength})`;
export const alertGlow = (strength: number) => `0 0 8px rgba(232,114,110,${strength})`;

/** The action button (A1): a thin capsule, its label in Audiowide; principal, secondary, danger, disabled. */
export const buttonRecipe = defineRecipe({
  className: 'rtype-button',
  base: {
    display: 'inline-flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: '8px',
    px: '20px',
    py: '11px',
    borderWidth: '1px',
    borderStyle: 'solid',
    borderRadius: 'l1',
    fontFamily: 'heading',
    fontWeight: 'normal',
    fontSize: '10px',
    lineHeight: 'normal',
    letterSpacing: '2px',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
    cursor: 'pointer',
    outline: 'none',
    textDecoration: 'none',
    transition: 'border-color 120ms, box-shadow 120ms, color 120ms, text-shadow 120ms',
    _focusVisible: { boxShadow: '0 0 0 1px rgba(255,255,255,0.7), 0 0 12px rgba(255,255,255,0.25)' },
    _disabled: {
      bg: 'transparent',
      bgImage: 'none',
      borderColor: 'border',
      color: 'fg.disabled',
      textShadow: 'none',
      boxShadow: 'none',
      cursor: 'not-allowed',
    },
  },
  variants: {
    variant: {
      primary: {
        bgImage: 'linear-gradient(180deg, rgba(255,255,255,0.18) 0%, rgba(255,255,255,0.05) 100%)',
        borderColor: 'rgba(255,255,255,0.55)',
        color: 'fg',
        textShadow: '0 0 10px rgba(255,255,255,0.6)',
        boxShadow: '0 0 7px rgba(255,255,255,0.18)',
        _hover: { borderColor: 'rgba(255,255,255,0.8)', boxShadow: '0 0 12px rgba(255,255,255,0.3)' },
      },
      secondary: {
        bg: 'transparent',
        borderColor: 'edge',
        color: 'fg.silver',
        _hover: { borderColor: 'rgba(255,255,255,0.45)', color: 'fg', textShadow: glow(0.5) },
      },
      danger: {
        bg: 'rgba(232,114,110,0.06)',
        borderColor: 'rgba(232,114,110,0.6)',
        color: 'alert',
        textShadow: '0 0 10px rgba(232,114,110,0.55)',
        boxShadow: '0 0 7px rgba(232,114,110,0.15)',
        _hover: {
          bg: 'rgba(232,114,110,0.12)',
          borderColor: 'alert',
          boxShadow: '0 0 12px rgba(232,114,110,0.3)',
        },
      },
    },
  },
  defaultVariants: { variant: 'secondary' },
});

/** An action of a line of a table: its word only, lit when hovered; red for what removes or ends. */
export const rowActionRecipe = defineRecipe({
  className: 'rtype-row-action',
  base: {
    display: 'inline-flex',
    alignItems: 'center',
    bg: 'transparent',
    border: 'none',
    p: '2px',
    fontFamily: 'heading',
    fontWeight: 'normal',
    fontSize: '9px',
    lineHeight: 'normal',
    letterSpacing: '1.6px',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
    cursor: 'pointer',
    outline: 'none',
    textDecoration: 'none',
    transition: 'color 120ms, text-shadow 120ms',
    _focusVisible: { textShadow: glow(0.9), textDecoration: 'underline' },
    _disabled: { color: 'fg.disabled', textShadow: 'none', cursor: 'not-allowed' },
  },
  variants: {
    tone: {
      light: { color: 'fg.silver', _hover: { color: 'fg', textShadow: glow(0.7) } },
      alert: { color: 'alert', _hover: { textShadow: alertGlow(0.8) } },
    },
  },
  defaultVariants: { tone: 'light' },
});

/** The state badge (A2): one word in a thin frame, never a dot. Active, neutral, alert, muted. */
export const badgeRecipe = defineRecipe({
  className: 'rtype-badge',
  base: {
    display: 'inline-flex',
    alignItems: 'center',
    flexShrink: 0,
    px: '8px',
    py: '4px',
    borderWidth: '1px',
    borderStyle: 'solid',
    borderRadius: '3px',
    bg: 'transparent',
    fontFamily: 'heading',
    fontWeight: 'normal',
    fontSize: '8px',
    lineHeight: 'normal',
    letterSpacing: '1.6px',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
  },
  variants: {
    tone: {
      active: { borderColor: 'rgba(255,255,255,0.45)', color: 'fg', textShadow: glow(0.6) },
      neutral: { borderColor: 'edge', color: 'fg.silver' },
      alert: { borderColor: 'rgba(232,114,110,0.6)', color: 'alert', textShadow: alertGlow(0.55) },
      muted: { borderColor: 'border', color: 'fg.subtle' },
    },
    // A badge that filters or chooses: a button, its pressed state the active tone
    interactive: {
      true: {
        cursor: 'pointer',
        outline: 'none',
        transition: 'border-color 120ms, color 120ms',
        _hover: { borderColor: 'rgba(255,255,255,0.6)', color: 'fg' },
        _focusVisible: { boxShadow: '0 0 0 1px rgba(255,255,255,0.6)' },
        '&[aria-pressed=true]': { borderColor: 'rgba(255,255,255,0.45)', color: 'fg', textShadow: glow(0.6) },
        _disabled: { borderColor: 'border', color: 'fg.disabled', textShadow: 'none', cursor: 'not-allowed' },
      },
    },
  },
  defaultVariants: { tone: 'neutral' },
});

/** The navigation entry (A0): normal, hovered, active; the current page carries the signal and the flare. */
export const navRecipe = defineRecipe({
  className: 'rtype-nav',
  base: {
    position: 'relative',
    display: 'flex',
    alignItems: 'center',
    gap: '14px',
    h: '40px',
    px: '16px',
    w: '100%',
    flexShrink: 0,
    color: 'fg.muted',
    fontFamily: 'heading',
    fontSize: '11px',
    lineHeight: 'normal',
    letterSpacing: '2.2px',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
    textDecoration: 'none',
    outline: 'none',
    transition: 'color 120ms, text-shadow 120ms',
    // The signal: a 2 × 16 px bar before the text
    _before: { content: '""', w: '2px', h: '16px', flexShrink: 0, bg: 'transparent' },
    // The flare along the foot of the entry
    _after: {
      content: '""',
      position: 'absolute',
      left: 0,
      right: 0,
      bottom: 0,
      h: '1px',
      bgImage: flareLine,
      boxShadow: flareGlow,
      opacity: 0,
      transition: 'opacity 120ms',
    },
    _hover: { color: 'fg.silver', _before: { bg: 'signal' }, _after: { opacity: 0.25 } },
    _focusVisible: { color: 'fg.silver', _before: { bg: 'signal' }, _after: { opacity: 0.25 } },
    '&[aria-current=page]': {
      color: 'fg',
      textShadow: '0 0 12px rgba(255,255,255,0.7)',
      bgImage: 'linear-gradient(90deg, rgba(255,255,255,0.07) 0%, rgba(255,255,255,0) 100%)',
      _before: { bg: 'white', boxShadow: '0 0 8px 0 rgba(255,255,255,0.9)' },
      _after: { opacity: 1 },
    },
  },
});

const fieldControl = {
  bg: 'bg',
  borderWidth: '1px',
  borderColor: 'edge',
  borderRadius: 'l1',
  color: 'fg.silver',
  fontFamily: 'body',
  fontSize: '13px',
  px: '14px',
  caretColor: 'white',
  _placeholder: { color: 'fg.subtle', fontSize: '12px' },
  _focusVisible: {
    outline: 'none',
    color: 'fg',
    borderColor: 'rgba(255,255,255,0.6)',
    boxShadow: '0 0 12px 0 rgba(255,255,255,0.2)',
  },
  _invalid: {
    color: 'fg',
    borderColor: 'rgba(232,114,110,0.7)',
    boxShadow: '0 0 12px 0 rgba(232,114,110,0.18)',
  },
};

const config = defineConfig({
  globalCss: {
    'html, body': {
      bg: 'bg',
      color: 'fg',
      fontFamily: 'body',
      fontSize: '12px',
      colorScheme: 'dark',
    },
    '::selection': { bg: 'rgba(255,255,255,0.2)' },
  },
  theme: {
    tokens: {
      colors: {
        rtype: {
          void: { value: '#000000' },
          surface: { value: '#0a0b0d' },
          line: { value: '#1e2023' },
          edge: { value: '#2e3135' },
          rail: { value: '#26292d' },
          group: { value: '#6b7177' },
          signal: { value: '#5c6166' },
          silver: { value: '#c3c8cd' },
          white: { value: '#ffffff' },
          muted: { value: '#8a9097' },
          hint: { value: '#868c92' },
          disabled: { value: '#33373b' },
          alert: { value: '#e8726e' },
        },
      },
      fonts: {
        heading: { value: "'Audiowide', sans-serif" },
        body: { value: "'Exo 2', sans-serif" },
      },
      radii: {
        l1: { value: '4px' },
        l2: { value: '6px' },
        l3: { value: '8px' },
      },
    },
    // Chakra's own components (dialogs, fields, tables, toasts) read these: black and silver everywhere
    semanticTokens: {
      colors: {
        bg: {
          DEFAULT: { value: '{colors.rtype.void}' },
          subtle: { value: '{colors.rtype.surface}' },
          muted: { value: '{colors.rtype.surface}' },
          emphasized: { value: '{colors.rtype.line}' },
          panel: { value: '{colors.rtype.void}' },
        },
        fg: {
          DEFAULT: { value: '{colors.rtype.white}' },
          silver: { value: '{colors.rtype.silver}' },
          muted: { value: '{colors.rtype.muted}' },
          subtle: { value: '{colors.rtype.hint}' },
          group: { value: '{colors.rtype.group}' },
          disabled: { value: '{colors.rtype.disabled}' },
        },
        border: {
          DEFAULT: { value: '{colors.rtype.line}' },
          muted: { value: '{colors.rtype.line}' },
          subtle: { value: '{colors.rtype.line}' },
          emphasized: { value: '{colors.rtype.edge}' },
        },
        edge: { value: '{colors.rtype.edge}' },
        signal: { value: '{colors.rtype.signal}' },
        alert: { value: '{colors.rtype.alert}' },
        rail: { value: '{colors.rtype.rail}' },
        gray: {
          focusRing: { value: '{colors.rtype.white}' },
        },
      },
    },
    textStyles: {
      // The title of the page, in chrome, its glow a drop shadow, which a text clipped to its background keeps
      pageTitle: {
        value: {
          fontFamily: 'heading',
          fontWeight: 'normal',
          fontSize: '22px',
          lineHeight: 'normal',
          letterSpacing: '1.5px',
          backgroundImage: chromeGradient,
          backgroundClip: 'text',
          color: 'transparent',
          filter: 'drop-shadow(0 0 12px rgba(255,255,255,0.35))',
        },
      },
      // The title of a card, in chrome
      cardTitle: {
        value: {
          fontFamily: 'heading',
          fontWeight: 'normal',
          fontSize: '11px',
          lineHeight: 'normal',
          letterSpacing: '2.2px',
          textTransform: 'uppercase',
          backgroundImage: chromeGradient,
          backgroundClip: 'text',
          color: 'transparent',
          filter: 'drop-shadow(0 0 8px rgba(255,255,255,0.25))',
        },
      },
      // The small labels: Exo 2 Medium, capitals, wide tracking
      caption: {
        value: {
          fontFamily: 'body',
          fontWeight: 500,
          fontSize: '10px',
          lineHeight: 'normal',
          letterSpacing: '1.8px',
          textTransform: 'uppercase',
        },
      },
      label: {
        value: {
          fontFamily: 'body',
          fontWeight: 500,
          fontSize: '9px',
          lineHeight: 'normal',
          letterSpacing: '1.8px',
          textTransform: 'uppercase',
        },
      },
      // A name lit: a game, a pilot, an address
      name: {
        value: {
          fontFamily: 'heading',
          fontWeight: 'normal',
          fontSize: '10px',
          lineHeight: 'normal',
          letterSpacing: '1px',
          textTransform: 'uppercase',
          color: 'fg',
          textShadow: '0 0 8px rgba(255,255,255,0.4)',
          whiteSpace: 'nowrap',
        },
      },
      // A number, or a value of the server
      number: {
        value: {
          fontFamily: 'heading',
          fontWeight: 'normal',
          fontSize: '10px',
          lineHeight: 'normal',
          letterSpacing: '1px',
          textTransform: 'uppercase',
          fontVariantNumeric: 'tabular-nums',
          whiteSpace: 'nowrap',
        },
      },
      // A link of a card's header, such as "See all"
      link: {
        value: {
          fontFamily: 'heading',
          fontWeight: 'normal',
          fontSize: '9px',
          lineHeight: 'normal',
          letterSpacing: '1.6px',
          textTransform: 'uppercase',
        },
      },
      body: { value: { fontFamily: 'body', fontWeight: 400, fontSize: '12px', lineHeight: 'normal' } },
      hint: { value: { fontFamily: 'body', fontWeight: 400, fontSize: '11px', lineHeight: 'normal' } },
    },
    layerStyles: {
      // A card of a screen: black, its border, a radius of 8 px
      card: {
        value: {
          bg: 'bg',
          borderWidth: '1px',
          borderColor: 'border',
          borderRadius: 'l3',
        },
      },
    },
    // The recipes of the panel's own controls are given to their components (components/Controls.tsx); here,
    // the fields of Chakra
    recipes: {
      input: {
        base: { ...fieldControl, h: '40px' },
        defaultVariants: { variant: 'outline', size: 'md' },
        variants: { variant: { outline: fieldControl }, size: { md: { h: '40px', textStyle: 'body' } } },
      },
    },
    slotRecipes: {
      field: {
        slots: fieldAnatomy.keys(),
        base: {
          root: { gap: '8px' },
          label: { textStyle: 'caption', color: 'fg.muted', fontWeight: 500 },
          helperText: { textStyle: 'hint', color: 'fg.subtle' },
          errorText: { textStyle: 'hint', color: 'alert' },
        },
      },
      dialog: {
        slots: dialogAnatomy.keys(),
        base: {
          backdrop: { bg: 'rgba(0,0,0,0.72)' },
          content: {
            bg: 'bg',
            borderWidth: '1px',
            borderRadius: 'l3',
            px: '32px',
            py: '28px',
            gap: '18px',
          },
          header: { p: 0 },
          body: { p: 0, color: 'fg.muted' },
          footer: { p: 0 },
        },
      },
      table: {
        slots: tableAnatomy.keys(),
        base: {
          root: { fontFamily: 'body', fontSize: '12px', bg: 'transparent', borderCollapse: 'collapse' },
          header: { bg: 'transparent' },
          columnHeader: {
            textStyle: 'label',
            color: 'fg.subtle',
            fontWeight: 500,
            h: '32px',
            px: '8px',
            py: 0,
            borderBottomWidth: '1px',
            borderColor: 'border',
            whiteSpace: 'nowrap',
          },
          cell: {
            h: '44px',
            px: '8px',
            py: 0,
            color: 'fg.muted',
            borderBottomWidth: '1px',
            borderColor: 'border',
          },
          row: {
            bg: 'transparent',
            transition: 'background 120ms',
            _hover: {
              bgImage: 'linear-gradient(90deg, rgba(255,255,255,0.05) 0%, rgba(255,255,255,0) 100%)',
            },
            '&[aria-selected=true]': {
              bgImage: 'linear-gradient(90deg, rgba(255,255,255,0.07) 0%, rgba(255,255,255,0.02) 100%)',
            },
            '&[data-tone=alert]': {
              bgImage: 'linear-gradient(90deg, rgba(232,114,110,0.1) 0%, rgba(232,114,110,0) 100%)',
            },
          },
        },
      },
    },
  },
});

export const system = createSystem(defaultConfig, config);
