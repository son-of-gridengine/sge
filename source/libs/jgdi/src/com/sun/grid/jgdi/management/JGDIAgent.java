/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
package com.sun.grid.jgdi.management;

import com.sun.grid.jgdi.jni.AbstractEventClient;
import com.sun.grid.jgdi.jni.JGDIBaseImpl;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Properties;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.LogManager;
import javax.management.Notification;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import com.sun.grid.jgdi.management.mbeans.JGDIJMX;
import com.sun.grid.jgdi.security.JGDIPrincipal;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.security.AccessController;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.logging.LogRecord;
import java.util.logging.Logger;
import javax.management.NotificationListener;
import javax.management.remote.JMXConnectionNotification;
import javax.management.remote.JMXConnectorServer;
import javax.security.auth.Subject;
import sun.management.jmxremote.ConnectorBootstrap;

/**
 * JGDI JMX agent class.
 */
public class JGDIAgent {

    private JMXConnectorServer mbeanServerConnector;
    // Platform MBeanServer used to register your MBeans
    private MBeanServer mbs;
    private final static Logger log = Logger.getLogger(JGDIAgent.class.getName());
    private static final JGDIAgent agent = new JGDIAgent();

    /**
     * JGDIAgent can not be instantiate from outside
     */
    private JGDIAgent() {
    }

    public void startMBeanServer() throws Exception {

        File managementFile = new File(getJmxDir(), "management.properties");

        logger.log(Level.FINE, "loading mbean server configuration from {0}", managementFile);

        final Properties props = new Properties();
        props.load(new FileInputStream(managementFile));

        final String portStr = props.getProperty(ConnectorBootstrap.PropertyNames.PORT);

        mbeanServerConnector = ConnectorBootstrap.initialize(portStr, props);
        mbeanServerConnector.addNotificationListener(new MyNotificationListener(), null, null);
        mbs = mbeanServerConnector.getMBeanServer();
        logger.log(Level.FINE, "starting mbean server");
        mbeanServerConnector.start();
        logger.log(Level.INFO, "mbean server started (port={0})", portStr);
    }

    private void stopMBeanServer() {
        if (mbeanServerConnector != null) {
            try {
                logger.log(Level.FINE, "stopping mbean server");
                mbeanServerConnector.stop();
            } catch (Exception ex) {
                logger.log(Level.WARNING, "cannot stop mbean server", ex);
            }
        }
    }

    public static String getUrl() {
        if (url == null) {
            throw new IllegalStateException("JGDIAgent.url is not initialized");
        }
        return url;
    }
    private static File sgeRoot;

    public static File getSgeRoot() {
        if (sgeRoot == null) {
            String sgeRootStr = System.getProperty("com.sun.grid.jgdi.sgeRoot");
            if (sgeRootStr == null) {
                throw new IllegalStateException("system properties com.sun.grid.jgdi.sgeRoot not found");
            }
            sgeRoot = new File(sgeRootStr);
        }
        return sgeRoot;
    }

    public static String getSgeCell() {
        String ret = System.getProperty("com.sun.grid.jgdi.sgeCell");
        if (ret == null) {
            throw new IllegalStateException("system properties com.sun.grid.jgdi.sgeCell not found");
        }
        return ret;
    }
    private static File jmxDir;

    public static File getJmxDir() {
        if (jmxDir == null) {
            jmxDir = new File(getSgeRoot(), getSgeCell() + File.separatorChar + "common" + File.separatorChar + "jmx");
        }
        return jmxDir;
    }

    private MBeanServer getMBeanServer() throws IOException {
        if (mbs == null) {
            throw new IllegalStateException("mbean server is not started");
        }
        return mbs;
    }
    // JGDI url string
    private static String url;
    private final static Logger logger = Logger.getLogger(JGDIAgent.class.getName());

    public static void main(String[] args) {
        try {

            if (args.length != 1) {
                logger.log(Level.SEVERE, "invalid arguments for JGDIAgent: JGDIAgent <jgdi connect url>");
                return;
            }
            url = args[0];

            try {
                FileOutputStream stdout = new FileOutputStream("jgdi.stdout", true);
                System.setOut(new PrintStream(stdout, true));
                logger.fine("stdout redirected to jgdi.stdout");
            } catch (Exception ex) {
                logger.log(Level.WARNING, "cannot redirect stdout to file jgdi.stdout", ex);
            }
            try {
                FileOutputStream stderr = new FileOutputStream("jgdi.stderr", true);
                System.setErr(new PrintStream(stderr, true));
                logger.fine("stderr redirected to jgdi.stderr");
            } catch (Exception ex) {
                logger.log(Level.WARNING, "cannot redirect stderr to file jgdi.stderr", ex);
            }

            try {
                agent.startMBeanServer();
            } catch (Exception ex) {
                logger.log(Level.SEVERE, "startup of mbean server failed", ex);
                return;
            }
            try {
                // The following code blocks until the shutdownMain method is called
                waitForShutdown();
            } catch (InterruptedException ex) {
                logger.log(Level.FINE, "JGDIAgent has been interrupted");
            } finally {
                logger.log(Level.INFO, "JGDIAgent is going down");
                try {
                    JGDISession.closeAllSessions();
                    AbstractEventClient.closeAll();
                    JGDIBaseImpl.closeAllConnections();
                } finally {
                    agent.stopMBeanServer();
                }
            }

        } catch (Throwable ex) {
            logger.log(Level.SEVERE, "unexpected error in JGDIAgent", ex);
        } finally {
            logger.log(Level.INFO, "JGDIAgent is down");
            LogManager.getLogManager().reset();
        }
    }
    
    private static final Lock shutdownLock = new ReentrantLock();
    private static final Condition shutdownCondition = shutdownLock.newCondition();
    private static boolean isRunning = true;

    private static void waitForShutdown() throws InterruptedException {
        log.entering("JGDIAgent", "waitForShutdown");
        shutdownLock.lock();
        try {
            while (isRunning) {
                shutdownCondition.await();
            }
        } finally {
            shutdownLock.unlock();
        }
        log.exiting("JGDIAgent", "waitForShutdown");
    }

    /**
     * Shutdown the JGDIAgent
     */
    public static void shutdown() {
        log.entering("JGDIAgent", "shutdown");
        shutdownLock.lock();
        try {
            isRunning = false;
            shutdownCondition.signalAll();
        } finally {
            shutdownLock.unlock();
        }
        log.exiting("JGDIAgent", "shutdown");
    }

    /**
     * Get the name of the session mbean
     * @param connectionId the id of the jmx connection
     * @return name of the session mbean or <code>null</code> if the connection id
     *         does not contain a session id
     */
    public static ObjectName getObjectNameFromConnectionId(String connectionId) {
        log.entering("JGDIAgent", "getObjectNameFromConnectionId", connectionId);
        long sessionId = getSessionIdFromConnectionId(connectionId);
        ObjectName ret = null;
        if(sessionId >= 0) {
            ret = getObjectNameForSessionMBean(sessionId);
        }
        log.exiting("JGDIAgent", "getObjectNameFromConnectionId", ret);
        return ret;
    }
    
    /**
     * Get the name of the session mbean
     * @param sessionId the session id
     * @return the name of the session mbean
     */
    public static ObjectName getObjectNameForSessionMBean(long sessionId) {
        log.entering("JGDIAgent", "getObjectNameForSessionMBean", sessionId);
        ObjectName ret = null;
        try {
            ret = new ObjectName(String.format("gridengine:type=JGDI,sessionId=%d", sessionId));
        } catch (Exception ex) {
            IllegalStateException ilse = new IllegalStateException("Invalid object name", ex);
            log.throwing("JGDIAgent", "getObjectNameForSessionMBean", ilse);
            throw ilse;
        }
        log.exiting("JGDIAgent", "getObjectNameForSessionMBean", ret);
        return ret;
    }
    
    /**
     * Get the session id out of the connection id
     * @param connectionId the connection id
     * @return the session id of <code>-1</code> if the connection id does not contain a session id
     */
    public static long getSessionIdFromConnectionId(String connectionId) {
        log.entering("JGDIAgent", "getSessionIdFromConnectionId", connectionId);
        long ret = -1;
        int startIndex = connectionId.indexOf(JGDIPrincipal.SESSION_ID_PREFIX);
        if (startIndex >= 0) {
            startIndex += JGDIPrincipal.SESSION_ID_PREFIX.length();
            int endIndex = connectionId.indexOf(JGDIPrincipal.SESSION_ID_SUFFIX, startIndex);
            if (endIndex > 0) {
                String sessionIdStr = connectionId.substring(startIndex, endIndex);
                try {
                    log.log(Level.FINE, "sessionIdStr = {0}", sessionIdStr);
                    ret = Long.parseLong(sessionIdStr);
                } catch (NumberFormatException ex) {
                    log.log(Level.WARNING, "Got invalid sessionId ({0})", sessionIdStr);
                }
            } else {
                log.log(Level.WARNING, "end of sessionId not found in connectionId ({0})", connectionId);
            }
        } else {
            log.log(Level.WARNING, "sessionId not found in connectionId ({0})", connectionId);
        }
        if (ret < -1) {
            log.log(Level.WARNING, "jmx connection id does not contain a jgdi session id: {0}", connectionId);
        }
        log.exiting("JGDIAgent", "getSessionIdFromConnectionId", ret);
        return ret;
    }
    
    private void registerSessionMBean(JGDISession session) {
        log.entering("JGDIAgent", "registerSessionMBean", session);
        try {
            JGDIJMX mbean = new JGDIJMX(session);
            ObjectName mbeanName = getObjectNameForSessionMBean(session.getId());
            getMBeanServer().registerMBean(mbean, mbeanName);
            log.log(Level.FINE, "mbean for session {0} registered", session.getId());
        } catch (Exception ex) {
            LogRecord lr = new LogRecord(Level.WARNING, "Can not register mbean for session {0}");
            lr.setParameters(new Object[]{session.getId()});
            lr.setThrown(ex);
            log.log(lr);
        }
        log.exiting("JGDIAgent", "registerSessionMBean");
    }

    private void unregisterSessionMBean(JGDISession session) {
        log.entering("JGDIAgent", "unregisterSessionMBean", session);
        try {
            ObjectName mbeanName = getObjectNameForSessionMBean(session.getId());
            getMBeanServer().unregisterMBean(mbeanName);
            log.log(Level.FINE, "mbean for session {0} unregistered", session.getId());
        } catch (Exception ex) {
            LogRecord lr = new LogRecord(Level.WARNING, "Can not unregister mbean for session {0}");
            lr.setParameters(new Object[]{session.getId()});
            lr.setThrown(ex);
            log.log(lr);
        }
        log.exiting("JGDIAgent", "unregisterSessionMBean");
    }
    

    class MyNotificationListener implements NotificationListener {

        public void handleNotification(Notification notification, Object handback) {

            if (notification instanceof JMXConnectionNotification) {
                
                JMXConnectionNotification jn = (JMXConnectionNotification) notification;

                if (log.isLoggable(Level.FINE)) {
                    Subject sub = Subject.getSubject(AccessController.getContext());
                    log.log(Level.FINE, "Got notification from client {0}, subject = {1}", new Object[]{jn.getConnectionId(), sub});
                }
                
                long sessionId = getSessionIdFromConnectionId(jn.getConnectionId());
                if(sessionId >= 0) {
                    if (JMXConnectionNotification.CLOSED.equals(jn.getType())) {
                        log.log(Level.FINE, "client connection {0} closed", jn.getConnectionId());
                        JGDISession session = JGDISession.closeSession(sessionId);
                        if(session != null) {
                            unregisterSessionMBean(session);
                        }
                    } else if (JMXConnectionNotification.FAILED.equals(jn.getType())) {
                        log.log(Level.FINE, "client connection {0} failed", jn.getConnectionId());
                        JGDISession session = JGDISession.closeSession(sessionId);
                        if(session != null) {
                            unregisterSessionMBean(session);
                        }
                    } else if (JMXConnectionNotification.NOTIFS_LOST.equals(jn.getType())) {
                        log.log(Level.WARNING, "client connection {0} losts notification", jn.getConnectionId());
                    } else if (JMXConnectionNotification.OPENED.equals(jn.getType())) {
                        if(log.isLoggable(Level.FINE)) {
                            log.log(Level.FINE, "client connection {0} opened", new Object [] { jn.getConnectionId() });
                        }
                        JGDISession session = JGDISession.createNewSession(sessionId, url);
                        registerSessionMBean(session);
                    }
                } else {
                    log.log(Level.WARNING, "Got a jmx connection without a session id: {0}", jn.getConnectionId());
                }
            }
        }
    }
}

