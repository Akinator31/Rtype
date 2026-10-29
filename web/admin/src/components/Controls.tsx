// The controls of the design's row 0: the action button (A1), the state badge (A2) and its pressable form for
// filters, the navigation entry (A0); and the actions of the lines of a table, a word lit when hovered.

import { chakra, type HTMLChakraProps, type RecipeVariantProps } from '@chakra-ui/react';
import { Link, NavLink, type NavLinkProps } from 'react-router';

import { badgeRecipe, buttonRecipe, navRecipe, rowActionRecipe } from '../theme/system';

const ButtonBase = chakra('button', buttonRecipe);
const ButtonLinkBase = chakra(Link, buttonRecipe);
const RowActionBase = chakra('button', rowActionRecipe);
const RowLinkBase = chakra(Link, rowActionRecipe);
const BadgeBase = chakra('span', badgeRecipe);
const ChipBase = chakra('button', badgeRecipe);

export type ButtonProps = HTMLChakraProps<'button'> & RecipeVariantProps<typeof buttonRecipe>;

/** Principal (lit), secondary (silver) or danger (red): a button of an action. */
export function Button({ type = 'button', ...props }: ButtonProps) {
  return <ButtonBase type={type} {...props} />;
}

/** A button that goes to a page of the panel. */
export const ButtonLink = ButtonLinkBase;

export type RowActionProps = HTMLChakraProps<'button'> & RecipeVariantProps<typeof rowActionRecipe>;

/** An action of a line: Detail, Message, End, Remove, Ban, Lift. */
export function RowAction({ type = 'button', ...props }: RowActionProps) {
  return <RowActionBase type={type} {...props} />;
}

/** An action of a line that goes to a page, such as the detail of a game. */
export const RowLink = RowLinkBase;

export type BadgeTone = 'active' | 'neutral' | 'alert' | 'muted';

/** A state, in one word. */
export function Badge({ tone = 'neutral', ...props }: HTMLChakraProps<'span'> & { tone?: BadgeTone }) {
  return <BadgeBase tone={tone} {...props} />;
}

/** A badge that filters or chooses: pressed, it is lit. */
export function Chip({
  pressed,
  tone = 'neutral',
  type = 'button',
  ...props
}: HTMLChakraProps<'button'> & { pressed: boolean; tone?: BadgeTone }) {
  return <ChipBase type={type} tone={tone} interactive aria-pressed={pressed} {...props} />;
}

const NavItemBase = chakra(NavLink, navRecipe);

/** An entry of the navigation, lit by the signal and the flare while its page is shown. */
export function NavItem(props: Omit<NavLinkProps, 'className' | 'style'> & HTMLChakraProps<'a'>) {
  return <NavItemBase {...props} />;
}
