// The menu button of the design (node 42:111): text only, lit by the flare. A button, or a link of the
// navigation, which the flare lights while its page is shown.

import { chakra, type HTMLChakraProps, type RecipeVariantProps } from '@chakra-ui/react';
import type { ReactNode } from 'react';
import { NavLink, type NavLinkProps } from 'react-router';

import { flareRecipe } from '../theme/system';

const FlareBase = chakra('button', flareRecipe);
const FlareNavBase = chakra(NavLink, flareRecipe);

type FlareVariants = RecipeVariantProps<typeof flareRecipe>;

export type FlareButtonProps = HTMLChakraProps<'button'> & FlareVariants & { children: ReactNode };

export function FlareButton({ children, type = 'button', ...props }: FlareButtonProps) {
  return (
    <FlareBase type={type} {...props}>
      <span data-part="label">{children}</span>
    </FlareBase>
  );
}

export type FlareLinkProps = Omit<NavLinkProps, 'children' | 'className' | 'style'> &
  FlareVariants & { children: ReactNode };

export function FlareLink({ children, ...props }: FlareLinkProps) {
  return (
    <FlareNavBase {...props}>
      <span data-part="label">{children}</span>
    </FlareNavBase>
  );
}
