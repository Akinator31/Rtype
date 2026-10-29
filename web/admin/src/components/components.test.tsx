import { ChakraProvider } from '@chakra-ui/react';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import type { ReactNode } from 'react';
import { describe, expect, it, vi } from 'vitest';

import { system } from '../theme/system';
import { Banner } from './Banner';
import { EventFeed } from './EventFeed';
import { FlareButton } from './Flare';
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
  it('fades its lines with their rank, the newest lit', () => {
    themed(
      <EventFeed
        empty="Nothing"
        items={['A', 'B', 'C', 'D', 'E', 'F'].map((name, index) => ({
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
      '0.8',
      '0.6',
      '0.4',
      '0.22',
      '0.22',
    ]);
    expect(lines[2]).toHaveAttribute('data-tone', 'alert');
  });

  it('says when it is empty', () => {
    themed(<EventFeed empty="Nothing yet" items={[]} />);
    expect(screen.getByText('Nothing yet')).toBeInTheDocument();
  });
});

describe('the banner', () => {
  it('is an alert in red, a status otherwise', () => {
    themed(
      <>
        <Banner title="Connection lost" detail="Reconnecting" tone="alert" />
        <Banner title="Sector 02" />
      </>,
    );
    expect(screen.getByRole('alert')).toHaveTextContent('Connection lost');
    expect(screen.getByRole('alert')).toHaveTextContent('Reconnecting');
    expect(screen.getByRole('status')).toHaveTextContent('Sector 02');
  });
});

describe('the flare button', () => {
  it('is a button that does nothing while disabled', async () => {
    const user = userEvent.setup();
    const clicked = vi.fn();
    themed(
      <>
        <FlareButton onClick={clicked}>Play</FlareButton>
        <FlareButton onClick={clicked} disabled>
          Quit
        </FlareButton>
      </>,
    );
    await user.click(screen.getByRole('button', { name: 'Play' }));
    await user.click(screen.getByRole('button', { name: 'Quit' }));
    expect(clicked).toHaveBeenCalledTimes(1);
    expect(screen.getByRole('button', { name: 'Play' })).toHaveAttribute('type', 'button');
  });
});
