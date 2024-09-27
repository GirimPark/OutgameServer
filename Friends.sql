/*DROP TABLE IF EXISTS [dbo].[Friends];*/
IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[Friends]') AND type in (N'U'))
BEGIN
    CREATE TABLE [dbo].[Friends] (
        UserName NVARCHAR(255) NOT NULL,
        FriendName NVARCHAR(255) NOT NULL,
        PRIMARY KEY (UserName, FriendName),
        FOREIGN KEY (UserName) REFERENCES [dbo].[User](Nickname),
        FOREIGN KEY (FriendName) REFERENCES [dbo].[User](Nickname)
    );
END