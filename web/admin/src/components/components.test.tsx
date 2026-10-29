import { ChakraProvider } from '@chakra-ui/react';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import type { ReactNode } from 'react';
import { describe, expect, it, vi } from 'vitest';

import { system } from '../theme/system';
import { AlertStrip } from './AlertStrip';
import { EventFeed } from './EventFeed';
import { Badge, Button, Chip } from './Controls';
import { Gauge } from './Gauge';

function themed(node: ReactNode) {
  return render(<ChakraProvider value={system}>{node}</ChakraProvider>);
}

describe('the gauge', () => {
  it('says how full it is, between 0 and 100', () => {
    themed(
      <>
        <Gauge value={0.5} label="Half" valueText="2 of 4" />
        <Gauge value={1.7} label="Over" />
        <Gauge value={Number.NaN} label="Unknown" />
      </>,
    );
    const half = screen.getByRole('meter', { name: 'Half' });
    expect(half).toHaveAttribute('aria-valuenow', '50');
    expect(half).toHaveAttribute('aria-valuetext', '2 of 4');
    expect(screen.getByRole('meter', { name: 'Over' })).toHaveAttribute('aria-valuenow', '100');
    expect(screen.getByRole('meter', { name: 'Unknown' })).toHaveAttribute('aria-valuenow', '0');
  });

  it('shows its sparkle where the light ends, the bright one once full', () => {
    const { container } = themed(
      <>
        <Gauge value={0} label="Empty" />
        <Gauge value={0.25} label="Quarter" />
        <Gauge value={1} label="Full" />
      </>,
    );
    const sparkles = container.querySelectorAll('img');
    // None on an empty rail
    expect(sparkles).toHaveLength(2);
    expect(sparkles[0]?.getAttribute('src')).toContain('gauge');
    expect(sparkles[1]?.getAttribute('src')).toContain('gauge-full');
  });
});

describe('the feed of events', () => {
  it('fades its lines with their rank, as the design, the newest lit', () => {
    themed(
      <EventFeed
        empty="Nothing"
        items={['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'].map((name, index) => ({
          key: index,
          tone: index === 2 ? 'alert' : 'info',
          subject: name,
          verb: 'joined',
        }))}
      />,
    );
    const lines = screen.getByRole('log').children;
    expect(Array.from(lines, (line) => getComputedStyle(line).opacity)).toEqual([
      '1',
      '0.9',
      '0.8',
      '0.65',
      '0.5',
      '0.38',
      '0.26',
      '0.26',
    ]);
    expect(lines[2]).toHaveAttribute('data-tone', 'alert');
  });

  it('says when it is empty', () => {
    themed(<EventFeed empty="Nothing yet" items={[]} />);
    expect(screen.getByText('Nothing yet')).toBeInTheDocument();
  });
});

describe('the strip', () => {
  it('is an alert in red, a status otherwise, with its action', async () => {
    const user = userEvent.setup();
    const retried = vi.fn();
    themed(
      <>
        <AlertStrip
          title="Connection lost"
          detail="Reconnecting"
          action={<Button onClick={retried}>Retry</Button>}
        />
        <AlertStrip title="Step 12" tone="info" />
      </>,
    );
    expect(screen.getByRole('alert')).toHaveTextContent('Connection lost');
    expect(screen.getByRole('alert')).toHaveTextContent('Reconnecting');
    expect(screen.getByRole('status')).toHaveTextContent('Step 12');
    await user.click(screen.getByRole('button', { name: 'Retry' }));
    expect(retried).toHaveBeenCalledTimes(1);
  });
});

describe('the controls', () => {
  it('a button does nothing while disabled', async () => {
    const user = userEvent.setup();
    const clicked = vi.fn();
    themed(
      <>
        <Button variant="primary" onClick={clicked}>
          Apply
        </Button>
        <Button variant="danger" onClick={clicked} disabled>
          Ban
        </Button>
      </>,
    );
    await user.click(screen.getByRole('button', { name: 'Apply' }));
    await user.click(screen.getByRole('button', { name: 'Ban' }));
    expect(clicked).toHaveBeenCalledTimes(1);
    expect(screen.getByRole('button', { name: 'Apply' })).toHaveAttribute('type', 'button');
  });

  it('a chip says whether it is pressed; a badge is a word', () => {
    themed(
      <>
        <Chip pressed>All · 3</Chip>
        <Chip pressed={false}>Stuck · 0</Chip>
        <Badge tone="alert">Stuck</Badge>
      </>,
    );
    expect(screen.getByRole('button', { name: 'All · 3' })).toHaveAttribute('aria-pressed', 'true');
    expect(screen.getByRole('button', { name: 'Stuck · 0' })).toHaveAttribute('aria-pressed', 'false');
    expect(screen.getByText('Stuck')).not.toHaveAttribute('role');
  });
});
