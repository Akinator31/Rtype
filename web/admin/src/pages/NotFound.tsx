import { Empty, PageHeader, Surface } from '../components/Layout';
import { strings } from '../strings';

export function NotFound() {
  return (
    <>
      <PageHeader caption="404" title={strings.notFound.title} />
      <Surface>
        <Empty>{strings.notFound.detail}</Empty>
      </Surface>
    </>
  );
}
