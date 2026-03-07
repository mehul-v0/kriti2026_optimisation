import { Outlet } from 'react-router-dom';
import Sidebar from './Sidebar';
import StepProgress from './StepProgress';
import ScrollToTop from './ScrollToTop';

export default function Layout() {
  return (
    <div className="flex h-screen w-full overflow-hidden bg-background-light dark:bg-background-dark">
      <ScrollToTop />
      <Sidebar />
      <main className="flex-1 overflow-y-auto">
        <StepProgress />
        <Outlet />
      </main>
    </div>
  );
}

