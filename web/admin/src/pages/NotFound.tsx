import { ButtonLink } from '../components/Controls';
import { Card, Empty } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { strings } from '../strings';

export function NotFound() {
  usePageTitle(strings.notFound.title);
  return (
    <Card alignItems="center">
      <Empty>{strings.notFound.detail}</Empty>
      <ButtonLink to="/">{strings.nav.overview}</ButtonLink>
    </Card>
  );
}
