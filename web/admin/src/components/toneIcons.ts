import alertIcon from '../assets/icons/alert.svg';
import bonusIcon from '../assets/icons/bonus.svg';
import infoIcon from '../assets/icons/info.svg';
import type { Tone } from '../live/state';

/** The icons of the tones of the design: information, bonus, alert. */
export const toneIcons: Record<Tone, string> = { info: infoIcon, bonus: bonusIcon, alert: alertIcon };
