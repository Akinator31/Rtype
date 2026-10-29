// The theme of the panel, after the Figma design "R-Type UI — Éléments retenus" (node 42:3), direction
// "Éclat stellaire": black and silver, light as the only accent, red for alerts. Every value of the design is
// set here once; the pages and the components use these tokens, text styles and recipes, never a value of
// their own. The reference: https://www.figma.com/design/RrrH0ic4ttdyDoc3nRnWBr/Untitled?node-id=42-3

import { createSystem, defaultConfig, defineConfig, defineRecipe } from '@chakra-ui/react';
import { dialogAnatomy, nativeSelectAnatomy, tableAnatomy } from '@chakra-ui/react/anatomy';

import alertSparkle from '../assets/flares/banner-alert.svg';
import buttonSparkle from '../assets/flares/button.svg';

/** The silver of the section titles, clipped to their text. */
export const chromeGradient =
  'linear-gradient(180deg, #ffffff 0%, #c3c8cd 42%, #5e6369 52%, #a3a8ae 64%, #f4f6f8 100%)';

/** The flare: a 1 px horizontal line, transparent → white 85 % → transparent. */
export const flareLine =
  'linear-gradient(90deg, rgba(255,255,255,0) 0%, rgba(255,255,255,0.85) 50%, rgba(255,255,255,0) 100%)';
export const alertFlareLine =
  'linear-gradient(90deg, rgba(232,114,110,0) 0%, rgba(232,114,110,0.85) 50%, rgba(232,114,110,0) 100%)';
export const flareGlow = '0 0 6px 0 rgba(255,255,255,0.7)';
export const alertFlareGlow = '0 0 6px 0 rgba(232,114,110,0.7)';

/** What lights a control when it is selected, hovered or focused: its flare, its text, its sparkle. */
const lit = {
  color: 'fg',
  textShadow: '0 0 12px rgba(255,255,255,0.75)',
  _before: { opacity: 1, width: 'calc(100% + 32px)', height: '1px', bottom: 'auto', top: '50%' },
  '& > [data-part=label]::after': { opacity: 1 },
};

/**
 * A control carried by the flare: the text of the design's menu button (Audiowide 13 px, capitals, 2.86 px),
 * no fill; lit by the flare when hovered or focused, a short 2 px line under its text when pressed.
 */
export const flareRecipe = defineRecipe({
  className: 'rtype-flare',
  base: {
    position: 'relative',
    display: 'inline-flex',
    alignItems: 'center',
    justifyContent: 'center',
    h: '36px',
    minW: 'auto',
    px: '24px',
    bg: 'transparent',
    border: 'none',
    borderRadius: '0',
    color: 'fg.muted',
    fontFamily: 'heading',
    fontWeight: 'normal',
    fontSize: '13px',
    letterSpacing: '2.86px',
    textTransform: 'uppercase',
    whiteSpace: 'nowrap',
    cursor: 'pointer',
    outline: 'none',
    textDecoration: 'none',
    transition: 'color 120ms, text-shadow 120ms',
    _before: {
      content: '""',
      position: 'absolute',
      left: '50%',
      top: '50%',
      width: 'calc(100% + 32px)',
      height: '1px',
      transform: 'translate(-50%, 0)',
      backgroundImage: 'var(--flare-line)',
      boxShadow: 'var(--flare-glow)',
      opacity: 0,
      pointerEvents: 'none',
      transition: 'opacity 120ms',
    },
    '& > [data-part=label]': { position: 'relative', zIndex: 1 },
    // The sparkle of the design, 8 px after the text; its SVG holds the room of its glow around it
    '& > [data-part=label]::after': {
      content: '""',
      position: 'absolute',
      left: 'calc(100% + 0.3px)',
      top: '50%',
      width: '47.43px',
      height: '27px',
      transform: 'translateY(-50%)',
      backgroundImage: `url(${buttonSparkle})`,
      backgroundSize: '100% 100%',
      opacity: 0,
      pointerEvents: 'none',
      transition: 'opacity 120ms',
    },
    '--flare-line': flareLine,
    '--flare-glow': flareGlow,
    _hover: lit,
    _focusVisible: lit,
    _selected: lit,
    '&[aria-current=page]': lit,
    _active: {
      color: 'fg',
      textShadow: '0 0 6px rgba(255,255,255,0.3)',
      _before: {
        opacity: 1,
        width: '29px',
        height: '2px',
        top: 'auto',
        bottom: '2px',
        borderRadius: '1px',
        backgroundImage: 'none',
        bg: 'white',
        boxShadow: '0 0 6px 0 rgba(255,255,255,0.8)',
      },
      '& > [data-part=label]::after': { opacity: 0 },
    },
    _disabled: {
      color: 'fg.disabled',
      cursor: 'not-allowed',
      textShadow: 'none',
      _before: { opacity: 0 },
      '& > [data-part=label]::after': { opacity: 0 },
    },
  },
  variants: {
    tone: {
      light: {},
      // Red for what cannot be undone: its text and its flare, never a fill
      alert: {
        color: 'alert',
        '--flare-line': alertFlareLine,
        '--flare-glow': alertFlareGlow,
        _hover: { color: 'alert', textShadow: '0 0 12px rgba(232,114,110,0.75)' },
        _focusVisible: { color: 'alert', textShadow: '0 0 12px rgba(232,114,110,0.75)' },
        // The red sparkle of the design's alert banner
        '& > [data-part=label]::after': {
          backgroundImage: `url(${alertSparkle})`,
          width: '41.71px',
          height: '25px',
          left: 'calc(100% + 1.1px)',
        },
      },
    },
    size: {
      md: {},
      sm: { h: '28px', px: '12px', fontSize: '11px', letterSpacing: '2.4px' },
    },
  },
  defaultVariants: { tone: 'light', size: 'md' },
});

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
          rail: { value: '#26292d' },
          silver: { value: '#c3c8cd' },
          white: { value: '#ffffff' },
          muted: { value: '#8a9097' },
          hint: { value: '#6b7177' },
          disabled: { value: '#33373b' },
          alert: { value: '#e8726e' },
        },
      },
      fonts: {
        heading: { value: "'Audiowide', sans-serif" },
        body: { value: "'Inter', sans-serif" },
      },
      radii: {
        l1: { value: '4px' },
        l2: { value: '8px' },
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
          panel: { value: '{colors.rtype.surface}' },
        },
        fg: {
          DEFAULT: { value: '{colors.rtype.white}' },
          muted: { value: '{colors.rtype.muted}' },
          subtle: { value: '{colors.rtype.hint}' },
          disabled: { value: '{colors.rtype.disabled}' },
        },
        border: {
          DEFAULT: { value: '{colors.rtype.line}' },
          muted: { value: '{colors.rtype.line}' },
          subtle: { value: '{colors.rtype.line}' },
          emphasized: { value: '{colors.rtype.disabled}' },
        },
        alert: { value: '{colors.rtype.alert}' },
        rail: { value: '{colors.rtype.rail}' },
        gray: {
          focusRing: { value: '{colors.rtype.white}' },
        },
      },
    },
    textStyles: {
      // A section title in chrome: its glow a drop shadow, which a text clipped to its background keeps
      chrome: {
        value: {
          fontFamily: 'heading',
          fontSize: '15px',
          letterSpacing: '1.5px',
          textTransform: 'uppercase',
          backgroundImage: chromeGradient,
          backgroundClip: 'text',
          color: 'transparent',
          filter: 'drop-shadow(0 0 8px rgba(255,255,255,0.3))',
        },
      },
      title: {
        value: {
          fontFamily: 'heading',
          fontSize: '40px',
          letterSpacing: '1px',
          backgroundImage: chromeGradient,
          backgroundClip: 'text',
          color: 'transparent',
          filter: 'drop-shadow(0 0 16px rgba(255,255,255,0.35))',
        },
      },
      label: {
        value: {
          fontFamily: 'heading',
          fontSize: '13px',
          letterSpacing: '2.86px',
          textTransform: 'uppercase',
        },
      },
      // The small labels: Inter Medium 10 px, capitals
      caption: {
        value: {
          fontFamily: 'body',
          fontWeight: 500,
          fontSize: '10px',
          letterSpacing: '1.8px',
          textTransform: 'uppercase',
        },
      },
      body: { value: { fontFamily: 'body', fontWeight: 400, fontSize: '12px' } },
      number: { value: { fontFamily: 'heading', letterSpacing: '1px', fontVariantNumeric: 'tabular-nums' } },
    },
    layerStyles: {
      // A surface: a gradient from #0a0b0d to black, its border, a radius of 8 px
      surface: {
        value: {
          bgImage: 'linear-gradient(180deg, {colors.rtype.surface} 0%, {colors.rtype.void} 100%)',
          borderWidth: '1px',
          borderColor: 'border',
          borderRadius: 'l2',
        },
      },
    },
    recipes: {
      flare: flareRecipe,
      input: {
        defaultVariants: { variant: 'flushed' },
        variants: {
          variant: {
            flushed: {
              borderColor: 'border.emphasized',
              _focusVisible: { borderColor: 'fg', boxShadow: '0 1px 6px -2px rgba(255,255,255,0.7)' },
            },
          },
        },
      },
      textarea: {
        defaultVariants: { variant: 'flushed' },
        variants: {
          variant: {
            flushed: {
              borderColor: 'border.emphasized',
              _focusVisible: { borderColor: 'fg', boxShadow: '0 1px 6px -2px rgba(255,255,255,0.7)' },
            },
          },
        },
      },
    },
    slotRecipes: {
      dialog: {
        slots: dialogAnatomy.keys(),
        base: {
          backdrop: { bg: 'rgba(0,0,0,0.8)' },
          content: {
            bg: 'transparent',
            bgImage: 'linear-gradient(180deg, {colors.rtype.surface} 0%, {colors.rtype.void} 100%)',
            borderWidth: '1px',
            borderColor: 'border',
            borderRadius: 'l2',
            boxShadow: 'none',
          },
          title: { textStyle: 'chrome' },
          body: { color: 'fg.muted' },
        },
      },
      table: {
        slots: tableAnatomy.keys(),
        base: {
          root: { fontFamily: 'body', fontSize: '12px', bg: 'transparent' },
          columnHeader: { textStyle: 'caption', color: 'fg.muted', fontWeight: 500, borderColor: 'border' },
          cell: { borderColor: 'border' },
          row: {
            bg: 'transparent',
            // Hovered, a row is lit by the flare along its foot
            _hover: {
              bgImage: flareLine,
              bgSize: '100% 1px',
              backgroundPosition: 'bottom',
              bgRepeat: 'no-repeat',
            },
          },
        },
      },
      nativeSelect: {
        slots: nativeSelectAnatomy.keys(),
        base: {
          field: { bg: 'bg', borderColor: 'border.emphasized' },
        },
      },
    },
  },
});

export const system = createSystem(defaultConfig, config);
