import { Outlet } from 'react-router-dom';
import Topbar from './Topbar';
import ScrollToTop from './ScrollToTop';

export default function Layout() {
  return (
    <div className="flex flex-col h-screen w-full overflow-hidden bg-[#06080B] relative">
      {/* Scanline overlay */}
      <div className="absolute inset-0 scanline pointer-events-none opacity-[0.03] z-[100]" />
      <ScrollToTop />
      <Topbar />
      <main className="flex-1 overflow-y-auto" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
        <Outlet />
      </main>
    </div>
  );
}

